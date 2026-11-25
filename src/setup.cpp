#define _WIN32_WINNT 0x0A00
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <commctrl.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <gdiplus.h>
#include <cmath>
namespace fs=std::filesystem;
static bool ru;
static const wchar_t* tr(const wchar_t* r,const wchar_t* e){return ru?r:e;}
static void extract(int id,const fs::path& to){
 auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(id),RT_RCDATA);
 if(!resource)throw 1;
 auto ptr=LockResource(LoadResource(nullptr,resource));DWORD size=SizeofResource(nullptr,resource);
 if(!ptr||!size)throw 1;
 auto temp=to;temp+=L".new";std::ofstream f(temp,std::ios::binary);
 f.write((const char*)ptr,size);f.close();if(!f||!MoveFileExW(temp.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING))throw 1;
}

using namespace Gdiplus;
namespace {
constexpr int width=560,height=386;
fs::path dest,standard;
std::wstring issue;
HWND go,choose;
bool finished=false,portable=false;
ULONGLONG opened;
UINT dpi=96;
int pixels(int logical){return MulDiv(logical,dpi,96);}
void scaleGraphics(Graphics& g){g.SetPageUnit(UnitPixel);g.ScaleTransform(dpi/96.f,dpi/96.f);}
RECT windowBounds(DWORD style){
 RECT r{0,0,pixels(width),pixels(height)};
 AdjustWindowRectExForDpi(&r,style,FALSE,0,dpi);return r;
}
void placeButtons(){
 if(choose)MoveWindow(choose,pixels(28),pixels(327),pixels(282)-pixels(28),pixels(368)-pixels(327),TRUE);
 if(go)MoveWindow(go,pixels(352),pixels(327),pixels(532)-pixels(352),pixels(368)-pixels(327),TRUE);
}
void rounded(Graphics& g,RectF r,float rad,Brush& brush){
 GraphicsPath path;float d=rad*2;path.AddArc(r.X,r.Y,d,d,180,90);path.AddArc(r.GetRight()-d,r.Y,d,d,270,90);
 path.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);path.AddArc(r.X,r.GetBottom()-d,d,d,90,90);path.CloseFigure();g.FillPath(&brush,&path);
}
void words(Graphics& g,const std::wstring& s,RectF r,float px,Color c,bool strong=false,bool center=false){
 Font font(L"Segoe UI",px,strong?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush ink(c);StringFormat fmt;
 fmt.SetAlignment(center?StringAlignmentCenter:StringAlignmentNear);fmt.SetLineAlignment(StringAlignmentCenter);fmt.SetTrimming(StringTrimmingEllipsisPath);
 g.DrawString(s.c_str(),(INT)s.size(),&font,r,&fmt,&ink);
}
void background(Graphics& g,float phase){
 g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.Clear(Color(255,20,22,29));
 const Color white(255,247,247,250),muted(255,185,191,204);
 RectF header(26,21,508,100);LinearGradientBrush gradient(header,Color(255,61,66,137),Color(255,27,75,92),6.f);rounded(g,header,17,gradient);
 RectF art(43,38,66,66);SolidBrush purple(Color(255,160,178,255));rounded(g,art,10,purple);
 auto state=g.Save();g.SetClip(RectF(47,42,58,58));Pen loops(Color(255,54,57,124),1.7f);
 for(int i=0;i<11;i++){float radius=9+3*i;float x=78+5*std::sin(phase+i*.13f);g.DrawEllipse(&loops,x-radius,69-radius,2*radius,2*radius);}g.Restore(state);
 words(g,L"WaveCard",RectF(124,34,245,39),29,white,true);
 words(g,L"OBS Studio 33  ·  Windows x64",RectF(126,75,365,22),14,Color(255,218,225,248));
 words(g,finished?tr(L"Источник установлен",L"Source installed"):tr(L"Яндекс Музыка в OBS",L"Yandex Music in OBS"),RectF(28,135,500,36),25,white,true);
 std::wstring msg=finished?tr(L"Перезапусти OBS. В панели «Источники» нажми + и выбери «Я.Музыка». Стиль меняется в свойствах источника.",L"Restart OBS. In Sources, choose + → Yandex Music. Choose your style in the source properties."):
 tr(L"Показывает обложку, название и исполнителя прямо на стриме. Перед установкой или обновлением закрой OBS.",L"Show artwork, title and artist on your stream. Close OBS before installing or updating.");
 words(g,msg,RectF(29,174,502,48),16,muted);
 SolidBrush panel(Color(255,37,40,51));rounded(g,RectF(28,231,504,66),10,panel);
 words(g,portable?tr(L"Переносной OBS",L"Portable OBS"):tr(L"Обычный OBS и Steam",L"Standard OBS and Steam"),RectF(42,239,476,23),14,white,true);
 words(g,dest.wstring(),RectF(42,262,475,25),13,muted);
 if(!issue.empty())words(g,issue,RectF(29,298,501,27),13,Color(255,255,166,148));
}
void button(Graphics& g,RectF r,bool primary,bool hover,bool down,bool focus,const wchar_t* label){
 g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
 Color color=primary?(down?Color(255,218,183,75):hover?Color(255,255,231,146):Color(255,247,212,103)):
 (hover?Color(255,65,68,82):Color(255,45,48,60));SolidBrush fill(color);rounded(g,r,10,fill);
 words(g,label,r,15,primary?Color(255,34,29,19):Color(255,238,239,243),true,true);
 if(focus){SolidBrush cue(primary?Color(255,77,61,19):Color(255,223,228,244));g.FillEllipse(&cue,r.X+12,r.Y+r.Height*.5f-2.5f,5.f,5.f);}
}
const wchar_t* mainLabel(){return finished?tr(L"Готово",L"Done"):tr(L"Установить",L"Install");}
const wchar_t* folderLabel(){return portable?tr(L"Вернуть обычную папку",L"Use standard folder"):tr(L"Выбрать переносной OBS…",L"Choose portable OBS…");}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){
 switch(msg){
 case WM_DPICHANGED:{
  dpi=HIWORD(w);auto at=reinterpret_cast<const RECT*>(l);RECT size=windowBounds((DWORD)GetWindowLongPtrW(h,GWL_STYLE));
  SetWindowPos(h,nullptr,at->left,at->top,size.right-size.left,size.bottom-size.top,SWP_NOACTIVATE|SWP_NOZORDER);
  placeButtons();RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);return 0;
 }
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{
  PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);
  {const RECT area=ps.rcPaint;int w=area.right-area.left,hp=area.bottom-area.top; if(w<=0||hp<=0){EndPaint(h,&ps);return 0;}
   Bitmap back(w,hp,PixelFormat32bppPARGB);
   {Graphics buffer(&back);buffer.TranslateTransform(-(float)area.left,-(float)area.top);scaleGraphics(buffer);background(buffer,(GetTickCount64()-opened)*.002f);}
   Graphics screen(dc);screen.SetPageUnit(UnitPixel);
   screen.DrawImage(&back,Rect(area.left,area.top,w,hp),0,0,w,hp,UnitPixel);}EndPaint(h,&ps);return 0;
 }
 case WM_TIMER:{if(w==1&&!IsIconic(h)){RECT cover{pixels(43),pixels(38),pixels(110),pixels(105)};InvalidateRect(h,&cover,FALSE);}return 0;}
 case WM_DRAWITEM:{auto v=(DRAWITEMSTRUCT*)l;Graphics g(v->hDC);g.Clear(Color(255,20,22,29));POINT point;GetCursorPos(&point);ScreenToClient(v->hwndItem,&point);
  scaleGraphics(g);button(g,RectF(0,0,v->rcItem.right*96.f/dpi,v->rcItem.bottom*96.f/dpi),v->CtlID==101,PtInRect(&v->rcItem,point),v->itemState&ODS_SELECTED,(v->itemState&ODS_FOCUS)&&!(v->itemState&ODS_NOFOCUSRECT)&&!(SendMessageW(h,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS),v->CtlID==101?mainLabel():folderLabel());return TRUE;}
 case WM_COMMAND:{
  if(LOWORD(w)==IDCANCEL){DestroyWindow(h);return 0;}
  if(LOWORD(w)==102){
   if(portable){dest=standard;portable=false;issue.clear();}
   else{BROWSEINFOW info{};info.hwndOwner=h;info.lpszTitle=tr(L"Корневая папка переносного OBS",L"Portable OBS root folder");info.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    auto id=SHBrowseForFolderW(&info);if(!id)return 0;wchar_t path[MAX_PATH]{};bool ok=SHGetPathFromIDListW(id,path);CoTaskMemFree(id);std::error_code err;
    if(!ok||!fs::is_regular_file(fs::path(path)/L"bin"/L"64bit"/L"obs64.exe",err)){issue=tr(L"В выбранной папке не найден OBS x64.",L"OBS x64 was not found in this folder.");}
    else{dest=fs::path(path)/L"plugins"/L"ya-music";portable=true;issue.clear();}}
   SetWindowTextW(choose,folderLabel());InvalidateRect(choose,nullptr,TRUE);
  }else if(LOWORD(w)==101||LOWORD(w)==IDOK){
   if(finished){DestroyWindow(h);return 0;}
   try{fs::create_directories(dest/L"data");extract(102,dest/L"data"/L"note.svg");extract(101,dest/L"ya-music.dll");finished=true;issue.clear();ShowWindow(choose,SW_HIDE);SetWindowTextW(go,mainLabel());}
   catch(...){issue=tr(L"Ошибка записи. Закрой OBS и повтори установку.",L"Could not write the plugin. Close OBS and try again.");}
  }
  InvalidateRect(h,nullptr,FALSE);return 0;}
 case WM_DESTROY:KillTimer(h,1);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,msg,w,l);
}
}
int WINAPI wWinMain(HINSTANCE app,HINSTANCE,LPWSTR,int){
 ru=PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_RUSSIAN;CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 ULONG_PTR token;GdiplusStartupInput input;if(GdiplusStartup(&token,&input,nullptr)!=Ok)return 1;
 PWSTR path=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_ProgramData,0,nullptr,&path)))return 1;
 standard=dest=fs::path(path)/L"obs-studio"/L"plugins"/L"ya-music";CoTaskMemFree(path);
 WNDCLASSW wc{};wc.hInstance=app;wc.lpfnWndProc=proc;wc.lpszClassName=L"WaveCardSetup";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
 DWORD style=WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;dpi=GetDpiForSystem();RECT r=windowBounds(style);
 HWND h=CreateWindowW(wc.lpszClassName,L"WaveCard · OBS",style,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,app,nullptr);
 if(!h)return 1;
 dpi=GetDpiForWindow(h);r=windowBounds(style);
 SetWindowPos(h,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
 auto make=[&](int id,int x,int w,const wchar_t* label){return CreateWindowW(L"BUTTON",label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,pixels(x),pixels(327),pixels(x+w)-pixels(x),pixels(368)-pixels(327),h,(HMENU)(INT_PTR)id,app,nullptr);};
 choose=make(102,28,254,folderLabel());go=make(101,352,180,mainLabel());opened=GetTickCount64();SetTimer(h,1,33,nullptr);
 SendMessageW(h,WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);ShowWindow(h,SW_SHOW);SetFocus(go);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}GdiplusShutdown(token);CoUninitialize();return 0;
}


