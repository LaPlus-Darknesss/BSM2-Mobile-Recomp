#include "bsm_cursor.h"
#include "arm64_context.h"
#include <SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>

namespace {
float cursorX=0,cursorY=0;
std::atomic<bool> haveCursor{false};
bool diagnostic=false;
std::mutex cursorMutex;
unsigned long calls=0;
template<class T>T Read(uint64_t base,size_t offset){T v;std::memcpy(&v,reinterpret_cast<void*>(base+offset),sizeof(v));return v;}
}
void BsmCursorDiagnostic(float x,float y){std::lock_guard<std::mutex> lock(cursorMutex);cursorX=x;cursorY=y;haveCursor=true;diagnostic=true;}
extern "C" uint64_t BsmCursorHeld(uint64_t native){return haveCursor&&!getenv("BSM2_TOUCH_CONTROLS")?native|1:native;}
void BsmCursorPoll(void* handle){
  if(diagnostic)return;
  auto* window=static_cast<SDL_Window*>(handle);
  if(SDL_GetMouseFocus()!=window)return; // Retain the last target while outside.
  int x,y,w,h,dw,dh;SDL_GetMouseState(&x,&y);
  SDL_GetWindowSize(window,&w,&h);SDL_GL_GetDrawableSize(window,&dw,&dh);
  if(w<=0||h<=0||x<0||y<0||x>=w||y>=h)return;
  std::lock_guard<std::mutex> lock(cursorMutex);
  cursorX=float(x)*dw/w;cursorY=float(y)*dh/h;haveCursor=true;
}
extern "C" void BsmCursorTarget(Arm64Ctx* ctx){
  if(!haveCursor||getenv("BSM2_TOUCH_CONTROLS"))return;
  float pixelX,pixelY;
  {std::lock_guard<std::mutex> lock(cursorMutex);pixelX=cursorX;pixelY=cursorY;}
  // Only CScreenGame's ordinary movement branch calls this manager method.
  // Preserve its cutscene/pause gating and native movement/clamping. No touch
  // flags or UI state are changed: left mouse remains a real touch gesture.
  const uint64_t base=ctx->image_base;
  const float scale=Read<float>(base,0x1055ea8);
  const uint64_t size=Read<uint64_t>(base,0x1058798);
  if(!size||!std::isfinite(scale)||scale<=0)return;
  int dimensions[2];std::memcpy(dimensions,reinterpret_cast<void*>(size),sizeof(dimensions));
  if(dimensions[0]<=0||dimensions[1]<=0)return;
  const float x=(pixelX-Read<int>(base,0x1055f20))/scale;
  const float y=(pixelY-Read<int>(base,0x1055f24))/scale;
  // Desktop pointing uses the visible playfield directly. The retail touch
  // caller adds horizontal reach, a nonlinear lower band and a 30-unit lift
  // to keep the monkey clear of a finger; those offsets are unnecessary here.
  const float targetX=x-float(dimensions[0]>>1);
  const float targetY=y-float(dimensions[1]>>1);
  if(!std::isfinite(targetX)||!std::isfinite(targetY))return;
  arc_s_w(ctx,0,targetX);arc_s_w(ctx,1,targetY);
  if(getenv("BSM2_TRACE_CURSOR")&&++calls%60==1)
    std::printf("[bsm cursor] screen %.1f %.1f logical %d %d target %.2f %.2f\n",pixelX,pixelY,dimensions[0],dimensions[1],targetX,targetY);
}
