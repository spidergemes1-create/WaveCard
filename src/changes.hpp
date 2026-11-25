#pragma once
// TypedEventHandler GUIDs generated from the Windows.Media.Control metadata.
inline constexpr GUID changeTypes[]={{0xf2ce2b7,0xafa7,0x5ed0,{0x8c,0xb6,0x8c,0x40,0xcf,0x9b,0x3a,0x5f}},{0x2bdf1426,0xd41f,0x5896,{0x89,0x7f,0xef,0xc0,0xb0,0xfa,0x73,0x92}},{0x2e2a8630,0xdc8c,0x530a,{0x97,0x46,0xbc,0x98,0x4d,0x4b,0x2,0x9e}}};
struct ChangeHandler : IUnknown {
 std::atomic<ULONG> references{1};
 HANDLE event=nullptr;
 explicit ChangeHandler(HANDLE target){check(DuplicateHandle(GetCurrentProcess(),target,GetCurrentProcess(),&event,EVENT_MODIFY_STATE,FALSE,0)?S_OK:E_FAIL);}
 ~ChangeHandler(){CloseHandle(event);}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{
  if(!out)return E_POINTER;*out=nullptr;
  if(id!=IID_IUnknown && id!=__uuidof(IAgileObject) && std::none_of(std::begin(changeTypes),std::end(changeTypes),[&](auto& supported){return id==supported;}))return E_NOINTERFACE;
  *out=static_cast<IUnknown*>(this);AddRef();return S_OK;
 }
 ULONG STDMETHODCALLTYPE AddRef()override{return ++references;}
 ULONG STDMETHODCALLTYPE Release()override{auto left=--references;if(!left)delete this;return left;}
 virtual HRESULT STDMETHODCALLTYPE Invoke(IInspectable*,IInspectable*){SetEvent(event);return S_OK;}
};
struct ChangeWatch {
 ComPtr<Manager> manager;ComPtr<Session> session;ComPtr<ChangeHandler> handler;
 INT64 list{},media{},play{};bool hasList=false,hasMedia=false,hasPlay=false;
 explicit ChangeWatch(HANDLE wake){handler.Attach(new ChangeHandler(wake));}
 void watchManager(Manager* next){
  if(manager.Get()==next)return;if(hasList)manager->removeList(list);
  manager=next;hasList=manager&&SUCCEEDED(manager->addList(handler.Get(),&list));
 }
 void watchSession(Session* next){
  if(session.Get()==next)return;
  if(hasMedia)session->removeMedia(media);if(hasPlay)session->removePlayback(play);
  session=next;hasMedia=session&&SUCCEEDED(session->addMedia(handler.Get(),&media));
  hasPlay=session&&SUCCEEDED(session->addPlayback(handler.Get(),&play));
 }
 ~ChangeWatch(){watchSession(nullptr);watchManager(nullptr);}
};
