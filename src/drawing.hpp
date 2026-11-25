#pragma once
#include <gdiplus.h>
#include <shlwapi.h>
#include <array>
using namespace Gdiplus;
inline void roundRect(GraphicsPath& path,RectF box,float radius){
 float d=std::min({box.Width,box.Height,2*radius});if(d<1){path.AddRectangle(box);return;}
 path.AddArc(box.X,box.Y,d,d,180,90);path.AddArc(box.GetRight()-d,box.Y,d,d,270,90);
 path.AddArc(box.GetRight()-d,box.GetBottom()-d,d,d,0,90);path.AddArc(box.X,box.GetBottom()-d,d,d,90,90);path.CloseFigure();
}
inline void label(Graphics& g,const wstring& text,RectF box,float size,bool bold,Color color,StringAlignment align=StringAlignmentNear){
 Font font(L"Segoe UI",size,bold?FontStyleBold:FontStyleRegular,UnitPixel);StringFormat fmt;
 fmt.SetTrimming(StringTrimmingEllipsisCharacter);fmt.SetAlignment(align);fmt.SetLineAlignment(StringAlignmentCenter);
 fmt.SetFormatFlags(StringFormatFlagsNoWrap);SolidBrush brush(color);
 g.DrawString(text.c_str(),(INT)text.size(),&font,box,&fmt,&brush);
}
inline Color theme(Bitmap* image){
 if(!image||!image->GetWidth()||!image->GetHeight())return Color(255,49,61,85);
 struct Bucket{double r=0,g=0,b=0,n=0;};std::array<Bucket,13> bins{};
 for(int y=0;y<12;y++)for(int x=0;x<12;x++){
  Color p;if(image->GetPixel((2*x+1)*image->GetWidth()/24,(2*y+1)*image->GetHeight()/24,&p)!=Ok||p.GetA()<120)continue;
  double r=p.GetR(),g=p.GetG(),b=p.GetB(),hi=std::max({r,g,b}),lo=std::min({r,g,b}),d=hi-lo;
  int slot=12;double n=.4;if(d>20){double hue=hi==r?(g-b)/d:hi==g?2+(b-r)/d:4+(r-g)/d;
   if(hue<0)hue+=6;slot=int(hue*2)%12;n=1+d/64;}
  auto& v=bins[slot];v.r+=r*n;v.g+=g*n;v.b+=b*n;v.n+=n;
 }
 auto v=*std::max_element(bins.begin(),bins.end(),[](auto a,auto b){return a.n<b.n;});
 if(!v.n)return Color(255,49,61,85);
 double r=v.r/v.n,g=v.g/v.n,b=v.b/v.n;
 // Set a minimum brightness for near-black artwork; cap relative luminance
 // so secondary text remains legible across the whole moving field.
 if(std::max({r,g,b})<22)return Color(255,88,86,106);
 double gain=165/std::max({1.,r,g,b});
 auto component=[&](double c){return std::clamp(15.+c*gain,0.,225.);};
 r=component(r);g=component(g);b=component(b);
 auto linear=[](double c){return std::pow((c/255.+.055)/1.055,2.4);};
 while(.2126*linear(r)+.7152*linear(g)+.0722*linear(b)>.151){r*=.97;g*=.97;b*=.97;}
 return Color(255,(BYTE)r,(BYTE)g,(BYTE)b);
}
inline gs_texture_t* upload(Bitmap& bmp){
 BitmapData data{};Rect rect(0,0,bmp.GetWidth(),bmp.GetHeight());
 if(bmp.LockBits(&rect,ImageLockModeRead,PixelFormat32bppPARGB,&data)!=Ok)return nullptr;
 auto bytes=(const uint8_t*)data.Scan0;auto tex=gs_texture_create(rect.Width,rect.Height,GS_BGRA,1,&bytes,0);bmp.UnlockBits(&data);return tex;
}
struct Layout{
 bool capsule;int w,h;float u,pad,art,left,right,barEnd,barY;
 Layout(int width,int height,bool pill=false):capsule(pill),w(width),h(height){
  u=std::min(float(h),w/3.1f);pad=.102f*u;art=.796f*u;left=pad+art+.13f*u;right=w-pad;barEnd=right-.75f*u;barY=h*.5f+(pill?.235f:.31f)*u;
 }
};
inline RectF titleBox(const Layout& a,bool timed){
 float inset=a.capsule?.12f*a.u:0;
 float y=a.h*.5f+(a.capsule?-.37f:(timed?-.385f:-.305f))*a.u;
 return RectF(a.left+inset,y,a.right-a.left-2*inset,(a.capsule?.43f:.37f)*a.u);
}
inline gs_texture_t* titleStrip(const wstring& title,float size,int w,int h,int from){
 Bitmap bmp(w,h,PixelFormat32bppPARGB);
 {Graphics g(&bmp);g.Clear(Color(0,0,0,0));g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
 Font font(L"Segoe UI",size,FontStyleBold,UnitPixel);SolidBrush white(Color::White);
 StringFormat fmt;fmt.SetFormatFlags(StringFormatFlagsNoWrap|StringFormatFlagsNoClip);
 fmt.SetLineAlignment(StringAlignmentCenter);g.DrawString(title.c_str(),(INT)title.size(),&font,RectF(-(float)from,0,10000000.f,(float)h),&fmt,&white);}
 return upload(bmp);
}
inline gs_texture_t* card(const Song& song,const Layout& a,int radius,Color& tint,int& titleWidth){
 ComPtr<IStream> stream;std::unique_ptr<Bitmap> img;
 if(song.cover){stream.Attach(SHCreateMemStream((BYTE*)song.cover->data(),(UINT)song.cover->size()));
  if(stream){img.reset(Bitmap::FromStream(stream.Get()));if(img->GetLastStatus()!=Ok)img.reset();}}
 Bitmap bmp(a.w,a.h,PixelFormat32bppPARGB);
 {Graphics g(&bmp);g.Clear(Color(0,0,0,0));g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
 tint=theme(img.get());
 
 RectF cover(a.pad,(a.h-a.art)/2,a.art,a.art);GraphicsPath outline;roundRect(outline,cover,a.art*radius/100);
 SolidBrush tile(Color(65,255,255,255));g.FillPath(&tile,&outline);
 if(img){g.SetClip(&outline);g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
  float side=std::min(img->GetWidth(),img->GetHeight());g.DrawImage(img.get(),cover,(img->GetWidth()-side)/2,(img->GetHeight()-side)/2,side,side,UnitPixel);g.ResetClip();}
 else label(g,L"♪",cover,a.u*.42f,false,Color::White,StringAlignmentCenter);
 if(song.paused){
  float d=a.art*.39f,x=cover.X+(a.art-d)*.5f,y=cover.Y+(a.art-d)*.5f;
  SolidBrush shade(Color(200,21,20,27)),white(Color::White);g.FillEllipse(&shade,x,y,d,d);
  for(float k:{.33f,.54f})g.FillRectangle(&white,x+d*k,y+d*.28f,d*.13f,d*.44f);
 }
 auto box=titleBox(a,song.duration>0);float size=a.u*(a.capsule?.192f:.218f);
 Font font(L"Segoe UI",size,FontStyleBold,UnitPixel);RectF measured;StringFormat fmt;fmt.SetFormatFlags(StringFormatFlagsNoWrap);
 g.MeasureString(song.title.c_str(),(INT)song.title.size(),&font,PointF(0,0),&fmt,&measured);
 titleWidth=(int)ceil(measured.Width);
 if(titleWidth<=box.Width)label(g,song.title,box,size,true,Color::White,a.capsule?StringAlignmentCenter:StringAlignmentNear);
 float artistY=a.capsule?a.barY-a.u*.14f:a.h*.5f+a.u*(song.duration?-.083f:-.003f);
 float artistEnd=a.capsule&&song.duration?a.barEnd:a.right;
 label(g,song.artist,RectF(a.left,artistY,artistEnd-a.left,.28f*a.u),a.u*.148f,false,Color(255,244,242,247));
 }return upload(bmp);
}
inline gs_texture_t* timeTexture(long long sec,int width,int height,float u){
 Bitmap bmp(width,height,PixelFormat32bppPARGB);
 {Graphics g(&bmp);g.Clear(Color(0,0,0,0));g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
 auto text=L"−"+std::to_wstring(sec/60)+L":"+(sec%60<10?L"0":L"")+std::to_wstring(sec%60);
 label(g,text,RectF(0,0,(float)width,(float)height),std::max(8.f,std::round(u*.164f)),true,Color::White,StringAlignmentFar);}
 return upload(bmp);
}

