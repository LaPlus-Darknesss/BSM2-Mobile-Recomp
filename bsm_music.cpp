// Java MediaPlayer replacement for the game's MP3 music assets.
#define MINIMP3_IMPLEMENTATION
#include "vendor/minimp3/minimp3_ex.h"
#include "arm64_context.h"
#include "jni_env.h"
#include "shim.h"
#include <SDL.h>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <chrono>
namespace {
struct Track{mp3dec_file_info_t pcm{};SDL_AudioDeviceID device=0;bool playing=false,loop=true;size_t position=0;float volume=1;uint64_t rendered=0;};
std::map<uint64_t,Track> tracks;
std::vector<uint64_t> Arguments(size_t slot,Arm64Ctx* c){
  std::vector<uint64_t> out;
  if(slot==35||slot==115||slot==62||slot==142){
    if(!c->x[3])return out;std::vector<unsigned char> cursor(arc::ShimVaListSize());memcpy(cursor.data(),reinterpret_cast<void*>(c->x[3]),cursor.size());
    for(int i=0;i<3;++i)out.push_back(arc::ShimVaNextInt(cursor.data()));
  }else if(slot==36||slot==116||slot==63||slot==143){if(c->x[3]){auto p=reinterpret_cast<const uint64_t*>(c->x[3]);for(int i=0;i<3;++i)out.push_back(p[i]);}}
  else for(int i=3;i<6;++i)out.push_back(c->x[i]);
  return out;
}
void Render(void* opaque,Uint8* stream,int bytes){
  auto& t=*static_cast<Track*>(opaque);memset(stream,0,bytes);if(!t.playing||!t.pcm.samples)return;
  auto out=reinterpret_cast<int16_t*>(stream);size_t remaining=size_t(bytes)/2,at=0;
  while(remaining){if(t.position==t.pcm.samples){if(!t.loop){t.playing=false;break;}t.position=0;}size_t n=std::min(remaining,t.pcm.samples-t.position);for(size_t i=0;i<n;++i)out[at+i]=int16_t(std::clamp(float(t.pcm.buffer[t.position+i])*t.volume,-32768.f,32767.f));t.position+=n;remaining-=n;at+=n;t.rendered+=n;}
}
}
bool BsmMusicJni(size_t slot,const char* method,Arm64Ctx* c){
  if(!method)return false;
  if(strcmp(method,"loadMusic")&&strcmp(method,"playMusic")&&strcmp(method,"playMusicNoLoop")&&strcmp(method,"pauseMusic")&&strcmp(method,"resumeMusic")&&strcmp(method,"killMusic")&&strcmp(method,"stopMusic")&&strcmp(method,"unloadMusic")&&strcmp(method,"setVolume"))return false;
  auto args=Arguments(slot,c);if(args.empty())return false;
  if(!strcmp(method,"loadMusic")){
    const char* name=reinterpret_cast<const char*>(args[0]);
    uint64_t handle=arc_jni_object();Track& t=tracks[handle];
    if(name&&arc_jni_owns(args[0])){
      std::filesystem::path path=std::filesystem::path(arc::ShimAssetRoot())/name;
      mp3dec_t decoder{};int rc=mp3dec_load(&decoder,path.string().c_str(),&t.pcm,nullptr,nullptr);
      if(!rc){SDL_InitSubSystem(SDL_INIT_AUDIO);SDL_AudioSpec spec{};spec.freq=t.pcm.hz;spec.channels=uint8_t(t.pcm.channels);spec.format=AUDIO_S16LSB;spec.samples=1024;spec.callback=Render;spec.userdata=&t;t.device=SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0);}
      fprintf(stderr,"[bsm music] load %s samples %zu rate %d channels %d device %u\n",name,t.pcm.samples,t.pcm.hz,t.pcm.channels,t.device);
    }
    c->x[0]=handle;return true;
  }
  auto it=tracks.find(args[0]);if(it==tracks.end()){c->x[0]=0;return true;}auto& t=it->second;
  if(!strcmp(method,"setVolume")){
    float volume=0;if(slot==62||slot==142){std::vector<unsigned char> cursor(arc::ShimVaListSize());memcpy(cursor.data(),reinterpret_cast<void*>(c->x[3]),cursor.size());arc::ShimVaNextInt(cursor.data());volume=float(arc::ShimVaNextDouble(cursor.data()));}
    else if(slot==63||slot==143){uint32_t bits=uint32_t(args[1]);memcpy(&volume,&bits,4);}else volume=float(c->q[0].f64[0]);
    if(t.device)SDL_LockAudioDevice(t.device);t.volume=std::isfinite(volume)?std::clamp(volume,0.f,1.f):0;if(t.device)SDL_UnlockAudioDevice(t.device);c->x[0]=0;return true;
  }
  if(t.device)SDL_LockAudioDevice(t.device);
  if(!strcmp(method,"playMusic")||!strcmp(method,"playMusicNoLoop")){t.loop=strcmp(method,"playMusicNoLoop")!=0;t.position=0;t.playing=true;}
  else if(!strcmp(method,"pauseMusic"))t.playing=false;
  else if(!strcmp(method,"resumeMusic"))t.playing=true;
  else{t.playing=false;t.position=0;}
  bool playing=t.playing;if(t.device){SDL_UnlockAudioDevice(t.device);SDL_PauseAudioDevice(t.device,playing?0:1);}
  c->x[0]=0;return true;
}
void BsmMusicPump(){
  static auto report=std::chrono::steady_clock::now();auto now=std::chrono::steady_clock::now();
  if(std::chrono::duration<double>(now-report).count()<2)return;report=now;
  for(auto& entry:tracks){auto& t=entry.second;if(t.device)SDL_LockAudioDevice(t.device);double seconds=t.pcm.hz&&t.pcm.channels?double(t.rendered)/(t.pcm.hz*t.pcm.channels):0;fprintf(stderr,"[bsm music perf] handle %llx playing %d volume %.3f played_seconds %.3f\n",(unsigned long long)entry.first,int(t.playing),t.volume,seconds);if(t.device)SDL_UnlockAudioDevice(t.device);}
}
