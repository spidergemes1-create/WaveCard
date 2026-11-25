#pragma once
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <shcore.h>
#include <objidl.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
using std::wstring;
inline void check(HRESULT hr) { if (FAILED(hr)) throw hr; }
// Prefixes of the public Windows.Media.Control ABI in Windows.Media.winmd.
// Unused slots keep the published method order. Only x64 is supported.
struct Timeline : IInspectable {
 virtual HRESULT Start(INT64*)=0; virtual HRESULT End(INT64*)=0;
 virtual void unusedMin()=0; virtual void unusedMax()=0;
 virtual HRESULT Position(INT64*)=0; virtual HRESULT Updated(INT64*)=0;
};
struct Playback : IInspectable {
 virtual void unusedControls()=0; virtual HRESULT Status(int*)=0;
 virtual void unusedType()=0; virtual void unusedRepeat()=0;
 struct Rate : IInspectable {virtual HRESULT Value(double*)=0;};
 virtual HRESULT GetRate(Rate**)=0;
};
template<class T> struct Operation : IInspectable {
 virtual void unusedPut()=0; virtual void unusedGet()=0; virtual HRESULT Results(T**)=0;
};
struct AsyncInfo : IInspectable { virtual void unusedId()=0; virtual HRESULT Status(int*)=0;
 virtual HRESULT Error(HRESULT*)=0; virtual HRESULT Cancel()=0; };
template<class T> ComPtr<T> finish(ComPtr<Operation<T>> op) {
 const GUID iid={0x00000036,0,0,{0xc0,0,0,0,0,0,0,0x46}};
 ComPtr<AsyncInfo> info; check(op->QueryInterface(iid,(void**)info.GetAddressOf()));
 auto deadline=GetTickCount64()+4000;
 for(unsigned count=0;GetTickCount64()<deadline;++count) {
  int status=0; check(info->Status(&status));
  if(status) { ComPtr<T> result; check(op->Results(&result)); return result; }
  Sleep(count<3?1:8);
 }
 info->Cancel(); throw HRESULT_FROM_WIN32(ERROR_TIMEOUT);
}
struct Thumbnail : IInspectable {virtual HRESULT Open(Operation<IInspectable>**)=0;};
struct Media : IInspectable {
 virtual HRESULT Title(HSTRING*)=0; virtual void unusedSubtitle()=0; virtual HRESULT AlbumArtist(HSTRING*)=0;
 virtual HRESULT Artist(HSTRING*)=0; virtual HRESULT Album(HSTRING*)=0; virtual void unusedNumber()=0;
 virtual void unusedGenres()=0; virtual void unusedCount()=0; virtual void unusedType()=0;
 virtual HRESULT Art(Thumbnail**)=0;
};
struct Session : IInspectable {
 virtual HRESULT Id(HSTRING*)=0; virtual HRESULT Properties(Operation<Media>**)=0;
 virtual HRESULT Times(Timeline**)=0; virtual HRESULT Info(Playback**)=0;
 virtual void unusedPlay()=0;
 virtual void unusedPause()=0;
 virtual void unusedStop()=0;
 virtual void unusedRecord()=0;
 virtual void unusedFastForward()=0;
 virtual void unusedRewind()=0;
 virtual void unusedNext()=0;
 virtual void unusedPrevious()=0;
 virtual void unusedChannelUp()=0;
 virtual void unusedChannelDown()=0;
 virtual void unusedToggle()=0;
 virtual void unusedRepeat()=0;
 virtual void unusedRate()=0;
 virtual void unusedShuffle()=0;
 virtual void unusedPosition()=0;
 virtual HRESULT addTimeline(IUnknown*,INT64*)=0; virtual HRESULT removeTimeline(INT64)=0;
 virtual HRESULT addPlayback(IUnknown*,INT64*)=0; virtual HRESULT removePlayback(INT64)=0;
 virtual HRESULT addMedia(IUnknown*,INT64*)=0; virtual HRESULT removeMedia(INT64)=0;
};
struct Sessions : IInspectable { virtual HRESULT At(UINT32,Session**)=0; virtual HRESULT Size(UINT32*)=0; };
struct Manager : IInspectable {virtual void unusedCurrent()=0; virtual HRESULT All(Sessions**)=0;
 virtual void unusedAddCurrent()=0;virtual void unusedRemoveCurrent()=0;
 virtual HRESULT addList(IUnknown*,INT64*)=0;virtual HRESULT removeList(INT64)=0;};
struct Factory : IInspectable {virtual HRESULT Request(Operation<Manager>**)=0;};
inline ComPtr<Manager> connectMedia() {
 const wchar_t name[]=L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";
 HSTRING_HEADER header; HSTRING str; check(WindowsCreateStringReference(name,ARRAYSIZE(name)-1,&header,&str));
 const GUID iid={0x2050c4ee,0x11a0,0x57de,{0xae,0xd7,0xc9,0x7c,0x70,0x33,0x82,0x45}};
 ComPtr<Factory> factory; check(RoGetActivationFactory(str,iid,(void**)factory.GetAddressOf()));
 ComPtr<Operation<Manager>> op; check(factory->Request(&op)); return finish(op);
}
template<class T> wstring readString(T* obj,HRESULT(T::*getter)(HSTRING*)) {
 HSTRING str=nullptr; check((obj->*getter)(&str)); UINT32 len=0;
 auto ptr=WindowsGetStringRawBuffer(str,&len); wstring result(ptr,len); WindowsDeleteString(str); return result;
}
inline std::vector<char> artwork(Media* media) {
 ComPtr<Thumbnail> ref; check(media->Art(&ref)); if(!ref) return {};
 ComPtr<Operation<IInspectable>> op; check(ref->Open(&op)); auto stream=finish(op);
 ComPtr<IStream> input; check(CreateStreamOverRandomAccessStream(stream.Get(),IID_IStream,(void**)input.GetAddressOf()));
 STATSTG stat{}; check(input->Stat(&stat,STATFLAG_NONAME));
 if(stat.cbSize.QuadPart>12*1024*1024 || !stat.cbSize.QuadPart) return {};
 std::vector<char> bytes((size_t)stat.cbSize.QuadPart); ULONG n=0;
 check(input->Read(bytes.data(),(ULONG)bytes.size(),&n)); bytes.resize(n); return bytes;
}


