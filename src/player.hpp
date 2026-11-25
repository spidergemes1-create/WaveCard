#pragma once
#include "media.hpp"
#include <chrono>
#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <cwctype>
inline long long millis(){using namespace std::chrono;return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();}
struct Song{
 wstring title,artist,album;long long duration=0,pos=0,anchor=0;double speed=1;bool paused=false;
 std::shared_ptr<std::vector<char>> cover;
 long long elapsed()const{return std::clamp<long long>(pos+(long long)(std::max(0LL,millis()-anchor)*speed),0,duration);}
 bool same(const Song& b)const{return title==b.title&&artist==b.artist&&album==b.album;}
};
inline bool isMusic(wstring id){
 std::transform(id.begin(),id.end(),id.begin(),[](wchar_t c){if(c>=L'А'&&c<=L'Я')return (wchar_t)(c+32);return (wchar_t)towlower(c);});
 return (id.find(L"yandex")!=id.npos&&id.find(L"music")!=id.npos)||id.find(L"яндекс музык")!=id.npos;
}
#include "changes.hpp"
struct Player{

 std::shared_ptr<Song> current=std::make_shared<Song>();
 std::atomic<bool> done{false},reset{false};
 HANDLE wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);std::thread thread;
 Player():thread([this]{run();}){}
 ~Player(){done=true;SetEvent(wake);thread.join();CloseHandle(wake);}
 void refresh(){reset=true;SetEvent(wake);}
 void run(){
  HRESULT hr=RoInitialize(RO_INIT_MULTITHREADED);ComPtr<Manager> manager;Song cached;
  {ChangeWatch watch(wake);long long retryArt=0,firstSeen=0,emptySince=0;unsigned retries=0;wstring lastPlayer;
  while(!done){
   auto s=std::make_shared<Song>();

   try{
    if(reset.exchange(false)){manager.Reset();cached={};retryArt=0;}
    check(hr);if(!manager)manager=connectMedia();watch.watchManager(manager.Get());ComPtr<Sessions> list;check(manager->All(&list));
    UINT32 count=0;check(list->Size(&count));
    ComPtr<Session> chosen,standby;ComPtr<Playback> info;int status=0;
    wstring chosenId;int best=-1;
    for(UINT32 i=0;i<count;i++){
     ComPtr<Session> session;check(list->At(i,&session));wstring id=readString(session.Get(),&Session::Id);
     if(!isMusic(id))continue;
     ComPtr<Playback> candidate;check(session->Info(&candidate));int state=0;
     if(!candidate||FAILED(candidate->Status(&state)))continue;
     bool active=state==4||state==5;
     int rank=(state==4?4:state==5?2:0)+(id==lastPlayer?1:0);
     if(rank<=best)continue;
     best=rank;chosen.Reset();standby.Reset();
     if(active){chosen=session;info=candidate;status=state;chosenId=id;}
     else standby=session;
    }
    watch.watchSession(chosen?chosen.Get():standby.Get());
    if(chosen){
     lastPlayer=chosenId;auto session=chosen;s->paused=status==5;
     ComPtr<Operation<Media>> op;check(session->Properties(&op));auto m=finish(op);if(!m)throw E_FAIL;
     s->title=readString(m.Get(),&Media::Title);s->artist=readString(m.Get(),&Media::Artist);
     if(s->artist.empty())s->artist=readString(m.Get(),&Media::AlbumArtist);
     s->album=readString(m.Get(),&Media::Album);
     if(!s->same(cached)){cached=*s;firstSeen=millis();retryArt=0;retries=0;}
     s->cover=cached.cover;ComPtr<Timeline> tl;if(SUCCEEDED(session->Times(&tl))&&tl){
      INT64 start=0,end=0,pos=0,updated=0;
      if(SUCCEEDED(tl->Start(&start))&&SUCCEEDED(tl->End(&end))&&SUCCEEDED(tl->Position(&pos))&&SUCCEEDED(tl->Updated(&updated))){
       s->duration=std::max<INT64>(0,(end-start)/10000);s->pos=(pos-start)/10000;
       s->anchor=updated/10000-11644473600000LL;if(s->anchor<=0||s->anchor>millis())s->anchor=millis();
      }
     }
     ComPtr<Playback::Rate> rate;double v=1;if(SUCCEEDED(info->GetRate(&rate))&&rate&&SUCCEEDED(rate->Value(&v))&&std::isfinite(v)&&v>=0&&v<=16)s->speed=v;
     if(s->paused)s->speed=0;

     if(!cached.cover&&millis()>=retryArt){
      retryArt=millis()+std::min(2000u,60u<<std::min(retries++,5u));try{auto bytes=artwork(m.Get());if(!bytes.empty())cached.cover=std::make_shared<std::vector<char>>(std::move(bytes));}catch(HRESULT){}
     }
     s->cover=cached.cover;
    }
   }catch(HRESULT){manager.Reset();}
   long long now=millis();
   bool pendingCover=!s->title.empty()&&!s->cover;
   auto previous=std::atomic_load(&current);
   // The desktop player may stop or briefly detach its session at a playlist boundary.
   // Keep the completed track at 0:00 while a replacement is being prepared.
   bool atEnd=previous->duration>0&&!previous->paused&&previous->duration-previous->elapsed()<=2200;
   long long grace=atEnd?6000:2500;
   bool awaiting=!previous->title.empty()&&s->title.empty();
   if(s->title.empty()){
    if(awaiting){
     if(!emptySince)emptySince=now;
     if(now-emptySince>=grace){std::atomic_store(&current,s);lastPlayer.clear();}
    }else emptySince=0;
   }else{
    emptySince=0;
    if(!pendingCover||now-firstSeen>=700)std::atomic_store(&current,s);
   }
   // Notifications wake this immediately; timeout recovers missed events.
   bool subscribed=watch.hasList&&(!watch.session||(watch.hasMedia&&watch.hasPlay));
   DWORD wait=subscribed?2000:200;
   if(pendingCover)wait=std::min(wait,(DWORD)std::max(1LL,retryArt-now));
   if(awaiting)wait=std::min<DWORD>(wait,150);
   WaitForSingleObject(wake,wait);
  }}
  manager.Reset();if(SUCCEEDED(hr))RoUninitialize();
 }
};
