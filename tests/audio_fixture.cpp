// Own PCM and mocked guest callback: proves short buffers refill without
// any game frame pumping. No commercial code or assets are used here.
#include "arm64_context.h"
#include <SDL.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <thread>
#include <chrono>
uint64_t BsmResolveAudio(const char*);
struct Itf{uint64_t* vt;void* owner;};
static Itf* queue;
static int16_t samples[256];
static std::atomic<unsigned> completions{0};
extern "C" void arc_register_native(uint64_t,const char*){}
extern "C" uint64_t arc_call_guest(uint64_t,const uint64_t*,int){
  ++completions;
  reinterpret_cast<uint32_t(*)(void*,const void*,uint32_t)>(queue->vt[0])(queue,samples,sizeof(samples));
  return 0;
}
static Itf* Interface(Itf* object,const char* name){
  void* id=*reinterpret_cast<void**>(BsmResolveAudio(name));Itf* result=nullptr;
  auto status=reinterpret_cast<uint32_t(*)(void*,void*,void**)>(object->vt[3])(object,id,reinterpret_cast<void**>(&result));
  return status?nullptr:result;
}
int main(){
  SDL_SetMainReady();SDL_setenv("SDL_AUDIODRIVER","dummy",1);
  Itf* object=nullptr;reinterpret_cast<uint32_t(*)(void**)>(BsmResolveAudio("slCreateEngine"))(reinterpret_cast<void**>(&object));
  Itf* engine=Interface(object,"SL_IID_ENGINE");if(!engine)return 2;
  uint32_t pcm[]={2,2,22050000,16,16,3,2};void* source[]={nullptr,pcm};Itf* player=nullptr;
  reinterpret_cast<uint32_t(*)(void*,void**,void**)>(engine->vt[2])(engine,reinterpret_cast<void**>(&player),source);
  queue=Interface(player,"SL_IID_BUFFERQUEUE");Itf* play=Interface(player,"SL_IID_PLAY");if(!queue||!play)return 3;
  for(int i=0;i<128;++i)samples[2*i]=samples[2*i+1]=int16_t(5000*std::sin(6.283185307*440*i/22050));
  reinterpret_cast<uint32_t(*)(void*,uint64_t,uint64_t)>(queue->vt[3])(queue,1,0);
  auto enqueue=reinterpret_cast<uint32_t(*)(void*,const void*,uint32_t)>(queue->vt[0]);enqueue(queue,samples,sizeof(samples));enqueue(queue,samples,sizeof(samples));
  reinterpret_cast<uint32_t(*)(void*,uint32_t)>(play->vt[0])(play,3);
  auto start=std::chrono::steady_clock::now();std::this_thread::sleep_for(std::chrono::milliseconds(1050));
  reinterpret_cast<uint32_t(*)(void*,uint32_t)>(play->vt[0])(play,2);
  double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();unsigned n=completions.load();
  reinterpret_cast<uint32_t(*)(void*)>(player->vt[6])(player);SDL_Quit();
  double seconds=double(n)*128/22050;
  printf("short-buffer completions=%u played_seconds=%.3f wall_seconds=%.3f ratio=%.3f\n",n,seconds,elapsed,seconds/elapsed);
  return seconds/elapsed>.85&&seconds/elapsed<1.15?0:1;
}
