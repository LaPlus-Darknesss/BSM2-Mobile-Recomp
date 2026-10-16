#include "bsm_platform.h"
#include "arm64_context.h"
#include "jni_env.h"
#include "shim.h"
#include <map>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <SDL.h>
#include <windows.h>
#include <mutex>
#include <vector>
uint64_t BsmResolveAudio(const char*);
uint64_t BsmResolveRandom(const char*);
uint64_t BsmResolveLibc(const char*);
bool BsmMusicJni(size_t,const char*,Arm64Ctx*);

namespace {
std::map<std::string,uint64_t> natives;
int width=540,height=960;
uint64_t windowToken=0x42534d32;
SDL_Window* sdlWindow=nullptr;SDL_GLContext rootContext=nullptr;bool mainContextClaimed=false;
std::mutex captureMutex;std::string capturePath;
uint64_t One(){return 1;}
uint64_t Zero(){return 0;}
uint64_t EglError(){return 0x3000;}
uint64_t NativeWindow(void*,void*){return reinterpret_cast<uint64_t>(&windowToken);}
uint64_t EglDisplay(void*){return 1;}
uint64_t EglInitialize(void*,int* major,int* minor){if(major)*major=1;if(minor)*minor=5;return 1;}
uint64_t EglChoose(void*,const int*,uint64_t* configs,int count,int* n){if(configs&&count)*configs=1;if(n)*n=1;return 1;}
uint64_t EglAttrib(void*,void*,int attr,int* value){if(value)*value=attr==0x3025?24:attr==0x3026?8:8;return 1;}
uint64_t EglQuery(void*,void*,int attr,int* value){if(value)*value=attr==0x3057?width:height;return 1;}
uint64_t EglContext(void*,void*,void* share,const int*){
  if(!mainContextClaimed){mainContextClaimed=true;fprintf(stderr,"[bsm egl] main context %p\n",rootContext);return reinterpret_cast<uint64_t>(rootContext);}
  SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT,0);auto context=SDL_GL_CreateContext(sdlWindow);
  using Share=BOOL(WINAPI*)(void*,void*);auto fn=reinterpret_cast<Share>(GetProcAddress(GetModuleHandleA("opengl32.dll"),"wglShareLists"));
  BOOL shared=share&&fn?fn(share,context):TRUE;
  fprintf(stderr,"[bsm egl] worker context %p share %p result %d thread %lu\n",context,share,int(shared),(unsigned long)SDL_ThreadID());
  return reinterpret_cast<uint64_t>(context);
}
uint64_t EglCurrent(void*,void*,void*,void* context){int rc=SDL_GL_MakeCurrent(sdlWindow,context);fprintf(stderr,"[bsm egl] make current %p thread %lu result %d %s\n",context,(unsigned long)SDL_ThreadID(),rc,rc?SDL_GetError():"");return rc==0;}
uint64_t EglSurface(void*,void*,void*,void*){return reinterpret_cast<uint64_t>(sdlWindow);}
uint64_t EglSwap(void*,void*){
  std::string path;{std::lock_guard<std::mutex> lock(captureMutex);path.swap(capturePath);}
  if(!path.empty()){
    using Read=void(APIENTRY*)(int,int,int,int,unsigned,unsigned,void*);using Store=void(APIENTRY*)(unsigned,int);
    auto read=reinterpret_cast<Read>(SDL_GL_GetProcAddress("glReadPixels"));auto store=reinterpret_cast<Store>(SDL_GL_GetProcAddress("glPixelStorei"));
    std::vector<unsigned char> pixels(size_t(width)*height*3);if(store)store(0x0d05,1);if(read)read(0,0,width,height,0x1907,0x1401,pixels.data());
    using Get=void(APIENTRY*)(unsigned,int*);using Error=unsigned(APIENTRY*)();auto get=reinterpret_cast<Get>(SDL_GL_GetProcAddress("glGetIntegerv"));auto error=reinterpret_cast<Error>(SDL_GL_GetProcAddress("glGetError"));int viewport[4]={},fbo=0,program=0;if(get){get(0xba2,viewport);get(0x8ca6,&fbo);get(0x8b8d,&program);}fprintf(stderr,"[bsm capture state] viewport %d %d %d %d fbo %d program %d error %x\n",viewport[0],viewport[1],viewport[2],viewport[3],fbo,program,error?error():0);
    if(FILE* f=fopen(path.c_str(),"wb")){fprintf(f,"P6\n%d %d\n255\n",width,height);for(int y=height-1;y>=0;--y)fwrite(pixels.data()+size_t(y)*width*3,1,size_t(width)*3,f);fclose(f);fprintf(stderr,"[bsm capture] %s\n",path.c_str());}
  }
  SDL_GL_SwapWindow(sdlWindow);return 1;
}
int SysProperty(const char* key,char* value){const char* v="";if(!strcmp(key,"ro.build.version.sdk"))v="28";else if(!strcmp(key,"ro.product.cpu.abi"))v="arm64-v8a";else if(!strcmp(key,"ro.product.model"))v="BSM2 Desktop Prototype";strcpy(value,v);return int(strlen(v));}
char* Stpcpy(char* d,const char* s){strcpy(d,s);return d+strlen(s);}
void Finite(Arm64Ctx* c){c->x[0]=std::isfinite(c->q[0].f64[0]);}
void IsnanF(Arm64Ctx* c){c->x[0]=std::isnan(c->q[0].f32[0]);}
template<class T> uint64_t Host(T fn,const char* name){uint64_t addr=reinterpret_cast<uint64_t>(fn);arc_register_native(addr,name);return addr;}
std::string Profile(){const char* p=getenv("BSM2_PROFILE_ROOT");return std::filesystem::path(p?p:"profile").generic_string();}
}
void BsmSetDimensions(int w,int h){width=w;height=h;}
void BsmBindWindow(void* window,void* context){sdlWindow=static_cast<SDL_Window*>(window);rootContext=context;}
void BsmCaptureNext(const char* path){std::lock_guard<std::mutex> lock(captureMutex);capturePath=path?path:"";}
uint64_t BsmNative(const char* name){auto i=natives.find(name);return i==natives.end()?0:i->second;}
size_t BsmNativeCount(){return natives.size();}
uint64_t BsmResolve(const char* name){
  if(uint64_t a=BsmResolveLibc(name))return a;
  if(uint64_t a=BsmResolveRandom(name))return a;
  if(uint64_t a=BsmResolveAudio(name))return a;
  if(!strcmp(name,"ANativeWindow_fromSurface"))return Host(&NativeWindow,name);
  if(!strcmp(name,"ANativeWindow_release")||!strcmp(name,"ANativeWindow_setBuffersGeometry"))return Host(&Zero,name);
  if(!strcmp(name,"eglGetDisplay"))return Host(&EglDisplay,name);
  if(!strcmp(name,"eglInitialize"))return Host(&EglInitialize,name);
  if(!strcmp(name,"eglChooseConfig"))return Host(&EglChoose,name);
  if(!strcmp(name,"eglGetConfigAttrib"))return Host(&EglAttrib,name);
  if(!strcmp(name,"eglQuerySurface"))return Host(&EglQuery,name);
  if(!strcmp(name,"eglCreateContext"))return Host(&EglContext,name);
  if(!strcmp(name,"eglMakeCurrent"))return Host(&EglCurrent,name);
  if(!strcmp(name,"eglCreateWindowSurface"))return Host(&EglSurface,name);
  if(!strcmp(name,"eglSwapBuffers"))return Host(&EglSwap,name);
  if(!strcmp(name,"eglGetError"))return Host(&EglError,name);
  if(!strcmp(name,"eglCreateContext")||!strcmp(name,"eglCreateWindowSurface")||!strcmp(name,"eglMakeCurrent")||!strcmp(name,"eglSwapBuffers")||!strcmp(name,"eglDestroySurface")||!strcmp(name,"eglDestroyContext"))return Host(&One,name);
  if(!strcmp(name,"__system_property_get"))return Host(&SysProperty,name);
  if(!strcmp(name,"getuid")||!strcmp(name,"getgid")||!strcmp(name,"getegid"))return Host(&Zero,name);
  if(!strcmp(name,"stpcpy"))return Host(&Stpcpy,name);
  if(!strcmp(name,"__isfinite")||!strcmp(name,"__isnanf")){
    auto fn=!strcmp(name,"__isfinite")?&Finite:&IsnanF;uint64_t a=reinterpret_cast<uint64_t>(fn);arc_register_ctx_native(a,name,fn);return a;
  }
  return 0;
}
bool BsmJniHandle(size_t slot,const char* method,Arm64Ctx* c){
  // Retaining a Java reference preserves the object identity. Allocating a
  // fresh stand-in here loses the MediaPlayer handle returned by loadMusic.
  if(slot==21||slot==25||slot==226){c->x[0]=c->x[1];return true;}
  if(slot==24){c->x[0]=c->x[1]==c->x[2];return true;}
  if(slot==215){
    struct Native{const char* name;const char* sig;uint64_t fn;};
    auto table=reinterpret_cast<const Native*>(c->x[2]);
    for(size_t i=0;i<c->x[3];++i){natives[table[i].name]=table[i].fn;fprintf(stderr,"[bsm jni] %s %s\n",table[i].name,table[i].sig);}
    c->x[0]=0;return true;
  }
  if(!method)return false;
  if(BsmMusicJni(slot,method,c))return true;
  if((slot>=58&&slot<=60)||(slot>=138&&slot<=140)){
    if(!strcmp(method,"getScreenSizeInches")){c->q[0].f64[0]=6.5;c->x[0]=0;return true;}
  }
  bool object=(slot>=34&&slot<=36)||(slot>=114&&slot<=116);
  bool number=(slot>=37&&slot<=60)||(slot>=117&&slot<=140);
  if(object){
    const char* answer=nullptr;std::string storage;
    if(!strcmp(method,"getLanguageCode"))answer="en";
    else if(!strcmp(method,"getCountryCode"))answer="US";
    else if(!strcmp(method,"getVersionName"))answer="1.10";
    else if(!strcmp(method,"getBundleName"))answer="com.ninjakiwi.supermonkey";
    else if(!strcmp(method,"getDeviceID")||!strcmp(method,"getUniqueID"))answer="bsm2-local-prototype";
    else if(strstr(method,"StoragePath")||!strcmp(method,"getExternalFilesPath")){storage=Profile();answer=storage.c_str();}
    else if(!strcmp(method,"getExecutablePath")){storage=arc::ShimAssetRoot();answer=storage.c_str();}
    if(answer){c->x[0]=arc_jni_string(answer);fprintf(stderr,"[bsm answer] %s -> %s\n",method,answer);return true;}
  }
  if(number){
    uint64_t value=0;bool found=true;
    if(!strcmp(method,"getVersionNumber"))value=1008851;
    else if(!strcmp(method,"getNativeAudioSampleRate"))value=48000;
    else if(!strcmp(method,"getNativeAudioFramesPerBuffer"))value=1024;
    else if(!strcmp(method,"getTotalSystemMemoryInBytes"))value=uint64_t(4)<<30;
    else if(!strcmp(method,"getMemoryUsage"))value=uint64_t(128)<<20;
    else if(!strcmp(method,"getStoragePermissionGranted"))value=1;
    else if(!strcmp(method,"isOnline")||!strcmp(method,"getNetworkType"))value=0;
    else if(!strcmp(method,"getDeviceBootTime"))value=arc::ShimMonotonicMillis();
    else found=false;
    if(found){c->x[0]=value;return true;}
  }
  return false;
}
