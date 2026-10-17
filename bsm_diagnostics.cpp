// Capture the original failure while the guest's ARM64 frame chain is intact.
// These hooks observe only: native validation and exception handling still run.
#include "arm64_context.h"
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cerrno>
#include <cstring>

void BsmDiagnosticStack(const Arm64Ctx*);
namespace {
bool Read(uint64_t at,void* destination,size_t size){
  SIZE_T got=0;
  return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),destination,size,&got)&&got==size;
}
void Text(uint64_t at,char* destination,size_t size){
  for(size_t i=0;i+1<size;++i){
    if(!Read(at+i,destination+i,1)){destination[i]=0;return;}
    if(!destination[i])return;
  }
  destination[size-1]=0;
}
void Stack(const Arm64Ctx* c){BsmDiagnosticStack(c);fflush(stdout);fflush(stderr);}
}
extern "C" void BsmTraceThrow(Arm64Ctx* c){
  const int saved=errno;
  const DWORD savedError=GetLastError();
  uint64_t name=0;char type[256]{};
  if(Read(c->x[1]+8,&name,sizeof name)&&name)Text(name,type,sizeof type);
  fprintf(stderr,"[bsm exception] thread=%lu type=%s caller=+%llx\n",GetCurrentThreadId(),type,(unsigned long long)(c->x[30]-c->image_base));
  // std::runtime_error stores the shared message pointer after its vtable.
  // Do not guess the layouts of other exception classes.
  if(!strcmp(type,"St13runtime_error")){
    uint64_t message=0;char text[512]{};
    if(Read(c->x[0]+8,&message,sizeof message)&&message){Text(message,text,sizeof text);fprintf(stderr,"[bsm exception] message=%s\n",text);}
  }
  Stack(c);SetLastError(savedError);errno=saved;
}
extern "C" void BsmTraceJsonType(Arm64Ctx* c){
  const int saved=errno;
  const DWORD savedError=GetLastError();
  int32_t raw=0;
  if(Read(c->x[0],&raw,sizeof raw)){
    uint32_t actual=uint32_t(raw)^uint32_t(raw<0?-1:0);
    if(actual==7)actual=4; // The native JSON library's unsigned integer tag.
    if(actual!=uint32_t(c->x[1])){
      fprintf(stderr,"[bsm json mismatch] thread=%lu actual=%u expected=%u caller=+%llx\n",GetCurrentThreadId(),actual,uint32_t(c->x[1]),(unsigned long long)(c->x[30]-c->image_base));
      Stack(c);
    }
  }
  SetLastError(savedError);errno=saved;
}
