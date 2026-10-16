// Minimal OpenSL ES buffer-queue backend over SDL2. Guest callbacks always
// return through arc_call_guest rather than executing ARM pointers as x64.
#include "arm64_context.h"
#include <SDL.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#include <deque>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <cmath>

namespace {
uint64_t ids[4]={0x42534d01,0x42534d02,0x42534d03,0x42534d04};
void* iid[4]={&ids[0],&ids[1],&ids[2],&ids[3]};
struct Audio;
struct Itf{uint64_t* vt;Audio* owner;};
struct Audio{
  Itf object{},engine{},play{},queue{},volume{};
  SDL_AudioDeviceID device=0;
  uint32_t state=1,index=0;
  uint64_t callback=0,context=0;
  int volumeMb=0;bool mute=false,dead=false;
  struct Buffer{std::vector<unsigned char> data;size_t position=0;};
  std::deque<Buffer> pending;
  uint64_t bytesProduced=0,bytesPlayed=0,callbacksDelivered=0,underruns=0;
  uint32_t rate=0,bytesPerFrame=0;
};
std::recursive_mutex audioMutex;
std::vector<Audio*> players;
uint64_t objvt[10],engvt[15],playvt[12],queuevt[4],volvt[9];
Audio* Owner(void* p){return static_cast<Itf*>(p)->owner;}
uint32_t OK(){return 0;}
uint32_t Unsupported(){return 12;}
uint32_t ObjState(void*,uint32_t* p){if(p)*p=2;return 0;}
uint32_t Destroy(void* self){auto p=Owner(self);SDL_AudioDeviceID device;{std::lock_guard<std::recursive_mutex> lock(audioMutex);p->dead=true;device=p->device;p->device=0;}if(device)SDL_CloseAudioDevice(device);return 0;}
uint32_t Interface(void* self,void* id,void** out){auto p=Owner(self);if(!out)return 2;
  if(id==iid[0])*out=&p->engine;else if(id==iid[1])*out=&p->play;else if(id==iid[2])*out=&p->queue;else if(id==iid[3])*out=&p->volume;else{*out=nullptr;return 12;}return 0;}
Audio* New(){auto p=new Audio();p->object={objvt,p};p->engine={engvt,p};p->play={playvt,p};p->queue={queuevt,p};p->volume={volvt,p};return p;}
uint32_t Engine(void** out){if(!out)return 2;*out=&New()->object;return 0;}
uint32_t Mix(void*,void** out){if(out)*out=&New()->object;return 0;}
void Render(void* opaque,Uint8* stream,int bytes){
  auto p=static_cast<Audio*>(opaque);memset(stream,0,bytes);size_t at=0;
  while(at<size_t(bytes)){
    uint64_t callback=0,context=0;
    {std::lock_guard<std::recursive_mutex> lock(audioMutex);if(p->dead||p->state!=3)return;
      if(p->pending.empty()){++p->underruns;return;}
      auto& b=p->pending.front();size_t n=std::min(size_t(bytes)-at,b.data.size()-b.position);
      float gain=p->mute?0.f:std::pow(10.f,float(p->volumeMb)/2000.f);
      if(gain==1.f)memcpy(stream+at,b.data.data()+b.position,n);
      else if(p->bytesPerFrame>=2){auto src=reinterpret_cast<const int16_t*>(b.data.data()+b.position);auto dst=reinterpret_cast<int16_t*>(stream+at);for(size_t i=0;i<n/2;++i)dst[i]=int16_t(std::clamp(src[i]*gain,-32768.f,32767.f));}
      b.position+=n;at+=n;p->bytesPlayed+=n;
      if(b.position==b.data.size()){p->pending.pop_front();++p->index;++p->callbacksDelivered;callback=p->callback;context=p->context;}
    }
    // Release the queue lock before the engine mixer runs and refills it.
    // This callback follows consumed samples, independently of game frames.
    if(callback){uint64_t args[]={reinterpret_cast<uint64_t>(&p->queue),context};arc_call_guest(callback,args,2);}
  }
}
uint32_t Player(void*,void** out,void** source){
  auto p=New();SDL_InitSubSystem(SDL_INIT_AUDIO);SDL_AudioSpec want{},have{};
  want.freq=48000;want.channels=2;want.format=AUDIO_S16LSB;want.samples=1024;
  if(source&&source[1]){auto f=static_cast<uint32_t*>(source[1]);if(f[0]==2){want.channels=uint8_t(f[1]);want.freq=int(f[2]/1000);want.format=f[3]==8?AUDIO_U8:AUDIO_S16LSB;}}
  want.callback=Render;want.userdata=p;
  p->device=SDL_OpenAudioDevice(nullptr,0,&want,&have,0);
  fprintf(stderr,"[bsm audio] PCM %d Hz %d channels device %u %s\n",want.freq,want.channels,p->device,p->device?"opened":SDL_GetError());
  p->rate=want.freq;p->bytesPerFrame=want.channels*(want.format==AUDIO_U8?1:2);
  {std::lock_guard<std::recursive_mutex> lock(audioMutex);players.push_back(p);}if(out)*out=&p->object;return 0;
}
uint32_t PlaySet(void* self,uint32_t state){auto p=Owner(self);SDL_AudioDeviceID device;{std::lock_guard<std::recursive_mutex> lock(audioMutex);p->state=state;device=p->device;}if(device)SDL_PauseAudioDevice(device,state==3?0:1);return 0;}
uint32_t PlayGet(void* self,uint32_t* state){std::lock_guard<std::recursive_mutex> lock(audioMutex);if(state)*state=Owner(self)->state;return 0;}
uint32_t Enqueue(void* self,const void* data,uint32_t bytes){std::lock_guard<std::recursive_mutex> lock(audioMutex);auto p=Owner(self);if(data&&bytes){Audio::Buffer b;b.data.assign(static_cast<const unsigned char*>(data),static_cast<const unsigned char*>(data)+bytes);p->pending.push_back(std::move(b));p->bytesProduced+=bytes;}static int left=3;if(left-->0)fprintf(stderr,"[bsm audio buffer] %u bytes, %.2f ms\n",bytes,p->rate&&p->bytesPerFrame?1000.*bytes/(p->rate*p->bytesPerFrame):0);return 0;}
uint32_t Clear(void* self){std::lock_guard<std::recursive_mutex> lock(audioMutex);auto p=Owner(self);p->pending.clear();return 0;}
uint32_t QueueState(void* self,uint32_t* out){std::lock_guard<std::recursive_mutex> lock(audioMutex);auto p=Owner(self);if(out){out[0]=uint32_t(p->pending.size());out[1]=p->index;}return 0;}
uint32_t Callback(void* self,uint64_t callback,uint64_t context){std::lock_guard<std::recursive_mutex> lock(audioMutex);auto p=Owner(self);p->callback=callback;p->context=context;return 0;}
uint32_t VolumeSet(void* self,int32_t mb){std::lock_guard<std::recursive_mutex> lock(audioMutex);Owner(self)->volumeMb=mb;return 0;}
uint32_t VolumeGet(void* self,int32_t* mb){std::lock_guard<std::recursive_mutex> lock(audioMutex);if(mb)*mb=Owner(self)->volumeMb;return 0;}
uint32_t MaxVolume(void*,int32_t* mb){if(mb)*mb=0;return 0;}
uint32_t MuteSet(void* self,uint32_t mute){std::lock_guard<std::recursive_mutex> lock(audioMutex);Owner(self)->mute=mute!=0;return 0;}
uint32_t MuteGet(void* self,uint32_t* mute){std::lock_guard<std::recursive_mutex> lock(audioMutex);if(mute)*mute=Owner(self)->mute;return 0;}
template<class T>uint64_t Fn(T fn,const char* name){uint64_t a=reinterpret_cast<uint64_t>(fn);arc_register_native(a,name);return a;}
void Init(){static bool done=false;if(done)return;done=true;
  std::fill_n(objvt,10,Fn(&OK,"slOK"));std::fill_n(engvt,15,Fn(&Unsupported,"slUnsupported"));std::fill_n(playvt,12,Fn(&OK,"slOK"));std::fill_n(queuevt,4,Fn(&OK,"slOK"));std::fill_n(volvt,9,Fn(&OK,"slOK"));
  objvt[2]=Fn(&ObjState,"slObjectState");objvt[3]=Fn(&Interface,"slGetInterface");objvt[6]=Fn(&Destroy,"slDestroy");engvt[2]=Fn(&Player,"slCreatePlayer");engvt[7]=Fn(&Mix,"slCreateMix");
  playvt[0]=Fn(&PlaySet,"slSetPlayState");playvt[1]=Fn(&PlayGet,"slGetPlayState");queuevt[0]=Fn(&Enqueue,"slEnqueue");queuevt[1]=Fn(&Clear,"slClear");queuevt[2]=Fn(&QueueState,"slQueueState");queuevt[3]=Fn(&Callback,"slRegisterCallback");
  volvt[0]=Fn(&VolumeSet,"slVolumeSet");volvt[1]=Fn(&VolumeGet,"slVolumeGet");volvt[2]=Fn(&MaxVolume,"slMaxVolume");volvt[3]=Fn(&MuteSet,"slMuteSet");volvt[4]=Fn(&MuteGet,"slMuteGet");
}
}
uint64_t BsmResolveAudio(const char* name){
  Init();
  if(!strcmp(name,"slCreateEngine"))return Fn(&Engine,name);
  const char* names[]={"SL_IID_ENGINE","SL_IID_PLAY","SL_IID_BUFFERQUEUE","SL_IID_VOLUME"};
  for(int i=0;i<4;++i)if(!strcmp(name,names[i]))return reinterpret_cast<uint64_t>(&iid[i]);
  return 0;
}
void BsmAudioPump(){
  static auto report=std::chrono::steady_clock::now();auto now=std::chrono::steady_clock::now();
  if(std::chrono::duration<double>(now-report).count()>2){report=now;std::lock_guard<std::recursive_mutex> lock(audioMutex);for(auto p:players)if(!p->dead)fprintf(stderr,"[bsm audio perf] produced %llu played %llu callbacks %llu underruns %llu pending %zu\n",(unsigned long long)p->bytesProduced,(unsigned long long)p->bytesPlayed,(unsigned long long)p->callbacksDelivered,(unsigned long long)p->underruns,p->pending.size());}
}
