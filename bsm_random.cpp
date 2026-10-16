// Android's rand/srand use the shared BSD random/srandom stream, not the
// Windows CRT's 15-bit stream. Default 128-byte state: degree 31, separation 3.
// Algorithm reference: Android 9 bionic upstream-netbsd random.c (BSD-3-Clause).
// Copyright/redistribution terms: vendor/bionic_random/LICENSE.
#include "arm64_context.h"
#include <cstdint>
#include <cstring>
#include <mutex>
#include <cstdlib>
#include <cstdio>

namespace {
struct Generator {
  uint32_t state[31]{};
  unsigned front=3,rear=0;
  Generator(){Seed(1);}
  uint32_t Next(){
    state[front]+=state[rear];
    const uint32_t result=state[front]>>1;
    front=(front+1)%31;rear=(rear+1)%31;
    return result;
  }
  void Seed(uint32_t seed){
    state[0]=seed;
    for(unsigned i=1;i<31;++i){
      const int64_t previous=int32_t(state[i-1]);
      int64_t next=16807*(previous%127773)-2836*(previous/127773);
      if(next<=0)next+=INT64_C(0x7fffffff);
      state[i]=uint32_t(next);
    }
    front=3;rear=0;
    for(unsigned i=0;i<310;++i)Next();
  }
};
std::mutex randomMutex;
Generator& Stream(){static Generator generator;return generator;}
int64_t Next(){std::lock_guard<std::mutex> lock(randomMutex);return Stream().Next();}
uint64_t Seed(uint32_t seed){std::lock_guard<std::mutex> lock(randomMutex);Stream().Seed(seed);return 0;}
}
uint64_t BsmResolveRandom(const char* name){
  uint64_t address=0;
  if(!std::strcmp(name,"rand")||!std::strcmp(name,"random"))address=reinterpret_cast<uint64_t>(&Next);
  else if(!std::strcmp(name,"srand")||!std::strcmp(name,"srandom"))address=reinterpret_cast<uint64_t>(&Seed);
  if(address)arc_register_native(address,name);
  return address;
}
