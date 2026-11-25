#define NOMINMAX
#include "player.hpp"
#include <obs-module.h>
#include "drawing.hpp"
#include <graphics/vec4.h>
#include "ambient.hpp"
OBS_DECLARE_MODULE()
static ULONG_PTR gdip;
static bool ru(){auto l=obs_get_locale();return l&&strncmp(l,"ru",2)==0;}
static const char* tr(const char* r,const char* e){return ru()?r:e;}
static std::shared_ptr<Player> sharedPlayer(){
 static std::weak_ptr<Player> weak;static std::mutex mutex;std::lock_guard<std::mutex> guard(mutex);
 auto p=weak.lock();if(!p){p=std::make_shared<Player>();weak=p;}return p;
}
struct Source{
 std::shared_ptr<Player> player=sharedPlayer();std::atomic<int> w{420},h{112},corner{8};gs_effect_t* ambient=nullptr;Color tint,oldTint;std::atomic<int> transition{2},duration{330},tempo{100};
 std::atomic<bool> drift{true};
 int builtW=0,builtH=0,builtCorner=-1,dir=1;
 Song shown;std::vector<wstring> history;size_t cursor=0;
 gs_texture_t *base=nullptr,*old=nullptr,*clock=nullptr;
 double start=0,lastFrame=0;float opacity=0;long long lastSec=-1,lastPosition=0;bool pulse=false;
 std::atomic<bool> capsule{false};bool builtCapsule=false;
 gs_texture_t* strip=nullptr;int titleWidth=0,stripFrom=-1;double scrollStart=0;
 ~Source(){obs_enter_graphics();gs_effect_destroy(ambient);gs_texture_destroy(base);gs_texture_destroy(old);gs_texture_destroy(clock);gs_texture_destroy(strip);obs_leave_graphics();}
 void direction(const wstring& key){
  if(!history.empty()&&cursor>0&&history[cursor-1]==key){dir=-1;--cursor;return;}
  dir=1;if(!history.empty()&&cursor+1<history.size()&&history[cursor+1]==key){++cursor;return;}
  if(!history.empty())history.resize(cursor+1);history.push_back(key);if(history.size()>24)history.erase(history.begin());cursor=history.size()-1;
 }
};
static void drawTexture(gs_texture_t* tex,int x=0,int y=0){
 if(!tex)return;auto effect=obs_get_base_effect(OBS_EFFECT_DEFAULT);
 while(gs_effect_loop(effect,"Draw"))obs_source_draw(tex,x,y,0,0,false);
}
static void rect(float x,float y,float right,float bottom){

 gs_render_start(false);gs_vertex2f(x,y);gs_vertex2f(right,y);gs_vertex2f(x,bottom);gs_vertex2f(right,bottom);gs_render_stop(GS_TRISTRIP);

}

static void progress(const Layout& a,float fraction,float opacity){

 auto effect=obs_get_base_effect(OBS_EFFECT_SOLID);auto color=gs_effect_get_param_by_name(effect,"color");

 float tip=a.left+(a.barEnd-a.left)*fraction,y=a.barY,half=std::max(.75f,a.u*.009f);

 gs_effect_set_color(color,(uint32_t(85*opacity)<<24)|0xffffff);while(gs_effect_loop(effect,"Solid"))rect(a.left,y-half,a.barEnd,y+half);

 gs_effect_set_color(color,(uint32_t(255*opacity)<<24)|0xffffff);while(gs_effect_loop(effect,"Solid")){

  rect(a.left,y-half,tip,y+half);

  float r=std::max(1.5f,a.u*.032f);gs_render_start(false);

  for(int i=0;i<24;i++){float t=i*.26179939f,z=t+.26179939f;gs_vertex2f(tip,y);gs_vertex2f(tip+r*cosf(t),y+r*sinf(t));gs_vertex2f(tip+r*cosf(z),y+r*sinf(z));}

  gs_render_stop(GS_TRIS);

 }

}

static void surface(Source& s,gs_texture_t* tex,Color tint,const Layout& a,double now,float f){
 if(!s.ambient){drawTexture(tex);return;}
 auto e=s.ambient;auto set=[e](const char* name,float x,float y,float z,float w){vec4 v;vec4_set(&v,x,y,z,w);gs_effect_set_vec4(gs_effect_get_param_by_name(e,name),&v);};
 float phase=s.drift?float(fmod(now*.00026179939*(s.tempo.load()/100.),6.2831853)):0.f;
 set("pill",a.left,a.h*.5f-.37f*a.u,a.right-a.left,a.capsule?.43f*a.u:0);
 set("fillInfo",s.shown.duration?float(s.shown.elapsed())/s.shown.duration:0,0,0,0);
 set("coverBox",a.pad/a.w,(a.h-a.art)*.5f/a.h,a.art/a.w,a.art/a.h);
 set("changeInfo",s.old?f:1.f,s.transition,s.dir,0);
 gs_effect_set_texture(gs_effect_get_param_by_name(e,"previousImage"),s.old?s.old:tex);
 set("layoutInfo",a.w,a.h,a.u*.16f,s.opacity);set("motion",.5f+.5f*sin(phase),.45f+.40f*cos(phase),0,0);
 auto blend=[&](int a,int b){return (s.old?a+(b-a)*f:b)/255.f;};
 float pulse=s.pulse?1.f+.12f*std::sin(f*3.141593f):1.f;
 set("palette",pulse*blend(s.oldTint.GetR(),tint.GetR()),pulse*blend(s.oldTint.GetG(),tint.GetG()),pulse*blend(s.oldTint.GetB(),tint.GetB()),1);
 gs_effect_set_texture(gs_effect_get_param_by_name(e,"image"),tex);
 while(gs_effect_loop(e,"Draw"))gs_draw_sprite(tex,0,a.w,a.h);
}
static void render(void* data,gs_effect_t*){
 auto& s=*(Source*)data;auto song=std::atomic_load(&s.player->current);
 double now=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
 float step=std::clamp(float((now-s.lastFrame)/180.),0.f,1.f);s.lastFrame=now;
 const Song* playing=song.get();
 if(song->title.empty()){
  s.opacity=std::max(0.f,s.opacity-step);
  if(s.opacity<=0||s.shown.title.empty()){s.shown={};s.lastPosition=0;return;}
  playing=&s.shown;
 }else{s.opacity=std::min(1.f,s.opacity+step);}
 Layout a(s.w,s.h,s.capsule);int corner=s.corner;long long pos=playing->elapsed();
 bool change=!playing->same(s.shown),layout=s.builtCapsule!=a.capsule||s.builtW!=a.w||s.builtH!=a.h||s.builtCorner!=corner;
 if(change||layout||s.shown.paused!=playing->paused||s.shown.cover!=playing->cover||!s.base||(s.shown.duration>0)!=(playing->duration>0)){
  Color nextTint;auto tex=card(*playing,a,corner,nextTint,s.titleWidth);if(!tex)return;
  gs_texture_destroy(s.old);s.old=nullptr;
  s.builtCapsule=a.capsule;gs_texture_destroy(s.strip);s.strip=nullptr;s.stripFrom=-1;
  if(change||layout)s.scrollStart=now;
  bool lateCover=!change&&!s.shown.cover&&playing->cover;
  if((change||lateCover)&&!layout&&!s.shown.title.empty())s.old=s.base;else gs_texture_destroy(s.base);
  s.oldTint=s.tint;s.tint=nextTint;s.base=tex;s.builtW=a.w;s.builtH=a.h;s.builtCorner=corner;s.lastSec=-1;
  if(lateCover){s.start=now;s.pulse=false;}
  if(change){s.direction(playing->title+L"\n"+playing->artist+L"\n"+playing->album);s.start=now;s.pulse=false;}
 }else if(playing->duration>3500&&pos<2400&&s.lastPosition>playing->duration-2500&&s.lastPosition-pos>2500){
  s.start=now;s.pulse=true;
 }
 s.lastPosition=pos;if(playing!=&s.shown)s.shown=*playing;
 float f=s.transition?std::min(1.f,float(now-s.start)/s.duration.load()):1.f;
 f=f*f*(3-2*f);
 if(f>=1&&s.old){gs_texture_destroy(s.old);s.old=nullptr;}
 surface(s,s.base,s.tint,a,now,f);
 auto box=titleBox(a,playing->duration>0);int view=(int)box.Width;
 if(view>0&&s.titleWidth>box.Width&&s.ambient){
  int excess=s.titleWidth-view;double travel=excess/(a.u*.28)*1000;
  double clock=fmod(now-s.scrollStart,2*(1300+travel));
  double position=clock<1300+travel?(clock-1300)/travel:1-(clock-2600-travel)/travel;
  int offset=(int)round(excess*std::clamp(position,0.,1.));int width=std::min(s.titleWidth,view+1024);
  if(!s.strip||offset<s.stripFrom||offset+view>s.stripFrom+width){
   s.stripFrom=std::clamp(offset-512,0,s.titleWidth-width);gs_texture_destroy(s.strip);
   s.strip=titleStrip(playing->title,a.u*(a.capsule?.192f:.218f),width,(int)ceil(box.Height),s.stripFrom);
  }
  if(s.strip){
   vec4 v;vec4_set(&v,0,0,0,s.opacity*(s.old?f:1.f));gs_effect_set_vec4(gs_effect_get_param_by_name(s.ambient,"layoutInfo"),&v);
   gs_effect_set_texture(gs_effect_get_param_by_name(s.ambient,"image"),s.strip);
   gs_matrix_push();gs_matrix_translate3f(round(box.X),round(box.Y),0);
   while(gs_effect_loop(s.ambient,"Overlay"))gs_draw_sprite_subregion(s.strip,0,offset-s.stripFrom,0,view,(int)ceil(box.Height));
   gs_matrix_pop();
  }
 }
 if(playing->duration>0){
  if(!a.capsule)progress(a,(float)((double)pos/playing->duration),s.opacity);

  long long sec=(playing->duration-pos+999)/1000;int tw=(int)ceil(a.right-a.barEnd),th=(int)ceil(a.u*.28f);
  if(sec!=s.lastSec){gs_texture_destroy(s.clock);s.clock=timeTexture(sec,tw,th,a.u);s.lastSec=sec;}
  if(s.opacity>=1.f||!s.ambient)drawTexture(s.clock,(int)a.barEnd,(int)(a.barY-th*.5f));
  else if(s.clock){
   vec4 overlay;vec4_set(&overlay,0,0,0,s.opacity);gs_effect_set_vec4(gs_effect_get_param_by_name(s.ambient,"layoutInfo"),&overlay);
   gs_effect_set_texture(gs_effect_get_param_by_name(s.ambient,"image"),s.clock);
   while(gs_effect_loop(s.ambient,"Overlay"))obs_source_draw(s.clock,(int)a.barEnd,(int)(a.barY-th*.5f),0,0,false);
  }
 }

}
static void defaults(obs_data_t* d){obs_data_set_default_int(d,"width",420);obs_data_set_default_int(d,"height",112);obs_data_set_default_int(d,"radius",8);
 obs_data_set_default_bool(d,"drift",true);
 obs_data_set_default_int(d,"tempo",100);
 obs_data_set_default_int(d,"track_effect",2);obs_data_set_default_int(d,"duration",330);}
static void update(void* data,obs_data_t* d){auto s=(Source*)data;s->w=std::clamp<long long>(obs_data_get_int(d,"width"),96,4096);s->h=std::clamp<long long>(obs_data_get_int(d,"height"),32,2160);s->corner=std::clamp<long long>(obs_data_get_int(d,"radius"),0,50);
 if(!obs_data_get_bool(d,"smooth_migrated")){
  if(obs_data_has_user_value(d,"track_effect")&&obs_data_get_int(d,"track_effect")==0){obs_data_set_int(d,"track_effect",2);obs_data_set_int(d,"duration",330);}
  obs_data_set_bool(d,"smooth_migrated",true);
 }
 auto setting=[d](const char* key,int low,int high){return (int)std::clamp<long long>(obs_data_get_int(d,key),low,high);};
 s->capsule=obs_data_get_int(d,"design")==1;
 s->drift=obs_data_get_bool(d,"drift");s->tempo=setting("tempo",25,200);
 s->transition=setting("track_effect",0,3);s->duration=setting("duration",100,800);}
static bool refresh(obs_properties_t*,obs_property_t*,void* data){if(data)((Source*)data)->player->refresh();return false;}
static obs_properties_t* properties(void* data){
 auto p=obs_properties_create();
 auto design=obs_properties_add_list(p,"design",tr("Стиль","Style"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
 obs_property_list_add_int(design,tr("Обычный","Classic"),0);obs_property_list_add_int(design,tr("Яндекс","Yandex"),1);
 obs_properties_add_int(p,"width",tr("Ширина, пикс.","Width, px"),96,4096,1);
 obs_properties_add_int(p,"height",tr("Высота, пикс.","Height, px"),32,2160,1);
 obs_properties_add_int_slider(p,"radius",tr("Скругление обложки, %","Cover corner radius, %"),0,50,1);
 obs_properties_add_bool(p,"drift",tr("Переливание фона","Background drift"));
 obs_properties_add_int_slider(p,"tempo",tr("Скорость переливания, %","Drift speed, %"),25,200,5);
 auto transitions=obs_properties_add_list(p,"track_effect",tr("Смена трека","Track transition"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
 obs_property_list_add_int(transitions,tr("Сдвиг обложки","Cover slide"),1);obs_property_list_add_int(transitions,tr("Растворение","Dissolve"),2);
 obs_property_list_add_int(transitions,tr("Приближение обложки","Cover zoom"),3);obs_property_list_add_int(transitions,tr("Мгновенно","Instant"),0);
 obs_properties_add_int_slider(p,"duration",tr("Длительность перехода, мс","Transition duration, ms"),100,800,10);
 obs_properties_add_button2(p,"refresh",tr("Обновить","Refresh"),refresh,data);return p;
}
static const char* icon(void*){return obs_module_file("note.svg");}
MODULE_EXPORT const char* obs_module_description(){return "Yandex Music now playing source for Windows";}
MODULE_EXPORT bool obs_module_load(){
 GdiplusStartupInput config;if(GdiplusStartup(&gdip,&config,nullptr)!=Ok)return false;
 obs_source_info i{};i.id="ya_music_card";i.type=OBS_SOURCE_TYPE_INPUT;i.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW;
 i.get_name=[](void*){return tr("Я.Музыка","Yandex Music");};i.get_defaults=defaults;i.get_properties=properties;i.update=update;
 i.create=[](obs_data_t* d,obs_source_t*)->void*{auto s=new Source;obs_enter_graphics();char* errors=nullptr;s->ambient=gs_effect_create(ambientEffect,"wavecard.effect",&errors);obs_leave_graphics();
 if(errors){blog(LOG_WARNING,"[WaveCard] %s",errors);bfree(errors);}update(s,d);return s;};
 i.destroy=[](void* s){delete (Source*)s;};i.get_width=[](void* s){return (uint32_t)((Source*)s)->w.load();};i.get_height=[](void* s){return (uint32_t)((Source*)s)->h.load();};
 i.video_render=render;i.icon_type=OBS_ICON_TYPE_CUSTOM;i.get_dark_icon=icon;i.get_light_icon=icon;obs_register_source(&i);return true;
}
MODULE_EXPORT void obs_module_unload(){if(gdip)GdiplusShutdown(gdip);}




