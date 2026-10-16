#include "arm64_context.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
uint64_t BsmResolveRandom(const char*);
extern "C" void arc_register_native(uint64_t,const char*){}
int main(){
  auto draw=reinterpret_cast<int64_t(*)()>(BsmResolveRandom("rand"));
  auto random=reinterpret_cast<int64_t(*)()>(BsmResolveRandom("random"));
  auto seed=reinterpret_cast<uint64_t(*)(uint32_t)>(BsmResolveRandom("srand"));
  auto srandom=reinterpret_cast<uint64_t(*)(uint32_t)>(BsmResolveRandom("srandom"));
  const int64_t android[]={55436735,1399865117,2032643283,571329216};
  seed(0x01020304);
  for(auto expected:android)if(draw()!=expected)return 1;
  srandom(0x01020304);
  for(auto expected:android)if(random()!=expected)return 2;
  seed(1);if(draw()!=1804289383||random()!=846930886)return 3;
  seed(110);uint64_t bins[4]{};int64_t minimum=INT64_MAX,maximum=0;
  for(int i=0;i<10000;++i){auto n=draw();if(n<0||n>0x7fffffff)return 4;++bins[n>>29];if(n<minimum)minimum=n;if(n>maximum)maximum=n;}
  for(auto count:bins)if(count<2000||count>3000)return 5;
  // Native CWeapon::Fire samples a full angle and radius rand()/INT_MAX *
  // randomSpawnRange. All three reported powers use range 20. A 15-bit host
  // can produce a radius no greater than 0.00030517 instead of 20.
  seed(110);unsigned quadrants[4]{};double farthest=0;
  for(int i=0;i<10000;++i){
    const double angle=double(draw())/2147483647.0*6.283185307179586;
    const double radius=double(draw())/2147483647.0*20.0;
    const double x=std::cos(angle)*radius,y=std::sin(angle)*radius;
    ++quadrants[(x>=0?1:0)|(y>=0?2:0)];if(radius>farthest)farthest=radius;
  }
  for(auto count:quadrants)if(count<2000||count>3000)return 6;
  if(farthest<19.9)return 7;
  std::printf("Shared radial-spawn regression: all four quadrants covered, maximum radius %.6f / 20; legacy maximum %.9f\n",farthest,32767.0/2147483647.0*20.0);
  std::printf("Android official seed vectors: 8 passed; shared rand/random state passed; 10000 draws range %lld..%lld, bins %llu/%llu/%llu/%llu; host RAND_MAX=%d\n",(long long)minimum,(long long)maximum,(unsigned long long)bins[0],(unsigned long long)bins[1],(unsigned long long)bins[2],(unsigned long long)bins[3],RAND_MAX);
  return 0;
}
