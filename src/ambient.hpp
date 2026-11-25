#pragma once
// OBS compiles this effect once per source. Foreground glyphs and artwork
// retain their original colors; only the background field moves.
constexpr char ambientEffect[] = R"fx(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform texture2d previousImage;
uniform float4 changeInfo;
uniform float4 coverBox;

uniform float4 layoutInfo;
uniform float4 palette;
uniform float4 pill;
uniform float4 fillInfo;
uniform float4 motion;
sampler_state linearSampler { Filter = Linear; AddressU = Clamp; AddressV = Clamp; };
struct Vertex { float4 pos : POSITION; float2 uv : TEXCOORD0; };
Vertex vs(Vertex v){v.pos=mul(float4(v.pos.xyz,1),ViewProj);return v;}
float4 ps(Vertex v) : TARGET {
 float2 uv=v.uv;
 float2 dim=layoutInfo.xy;
 float radius=layoutInfo.z;
 float2 q=abs((uv-.5)*dim)-(.5*dim-radius);
 float mask=saturate(.5-(length(max(q,float2(0,0)))+min(max(q.x,q.y),0.0)-radius));
 float2 drift=(uv-motion.xy)*float2(1.55,.78);
 float glow=saturate(1-dot(drift,drift));
 float3 bg=palette.rgb*(.61+.39*glow*glow);
 if(pill.w>0.){
  float2 xy=uv*dim-pill.xy;
  float rad=pill.w*.5;
  float2 d=xy-float2(clamp(xy.x,rad,pill.z-rad),rad);
  float inside=saturate(rad-length(d)+.5);
  float filled=saturate(pill.z*fillInfo.x-xy.x+.5);
  bg=lerp(bg,float3(.90,.94,.97),inside*(.12+.20*filled));
 }
 float4 fg=image.Sample(linearSampler,uv);
 if(changeInfo.x<1.){
  float f=changeInfo.x;
  float2 lo=coverBox.xy,hi=lo+coverBox.zw;
  float2 prevUV=uv,nextUV=uv;
  if(all(uv>=lo)&&all(uv<hi)){
   if(changeInfo.y<1.5){prevUV.x+=coverBox.z*f*changeInfo.z;nextUV.x-=coverBox.z*(1.-f)*changeInfo.z;}
   else if(changeInfo.y>2.5){prevUV=(uv-(lo+hi)*.5)/(1.+.12*f)+(lo+hi)*.5;nextUV=(uv-(lo+hi)*.5)/(1.12-.12*f)+(lo+hi)*.5;}
   float4 before=previousImage.Sample(linearSampler,clamp(prevUV,lo,hi));
   float4 after=image.Sample(linearSampler,clamp(nextUV,lo,hi));
   if(changeInfo.y<1.5){fg=(all(nextUV>=lo)&&all(nextUV<hi))?after:before;}
   else fg=lerp(before,after,f);
  }else{fg=lerp(previousImage.Sample(linearSampler,uv),fg,f);}
 }
 return float4((fg.rgb+bg*(1-fg.a))*mask*layoutInfo.w,mask*layoutInfo.w);
}
float4 overlay(Vertex v):TARGET{return image.Sample(linearSampler,v.uv)*layoutInfo.w;}
technique Overlay {pass {vertex_shader=vs(v);pixel_shader=overlay(v);}}
technique Draw {pass {vertex_shader=vs(v); pixel_shader=ps(v);}}
)fx";
