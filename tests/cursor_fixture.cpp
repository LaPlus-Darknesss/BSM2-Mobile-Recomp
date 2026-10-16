// Asset-free checks of coordinate mapping and register isolation.
#include "arm64_context.h"
#include "../bsm_cursor.h"
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
extern "C" uint64_t BsmCursorHeld(uint64_t);
template<class T>void Put(std::vector<unsigned char>& image,size_t at,T value){std::memcpy(image.data()+at,&value,sizeof(value));}
int main(){
  std::vector<unsigned char> image(0x1060000);
  int size[]={540,960};
  Put(image,0x1058798,reinterpret_cast<uint64_t>(size));
  Put(image,0x1055ea8,2.0f);Put(image,0x1055f20,20);Put(image,0x1055f24,40);
  Put(image,0xaea860,1.25);Put(image,0xaea858,0.25);
  struct Case{float x,y,wx,wy;};
  Case cases[]={{560,1000,0,0},{160,600,-200,-200},{960,1400,200,200},{560,1240,0,120}};
  for(auto test:cases){
    Arm64Ctx c{};c.image_base=reinterpret_cast<uint64_t>(image.data());c.x[0]=0x123456789;c.nf=1;c.zf=0;c.cf=1;c.vf=0;c.q[2].u64[0]=0xabcdef;
    BsmCursorDiagnostic(test.x,test.y);BsmCursorTarget(&c);
    if(std::fabs(c.q[0].f32[0]-test.wx)>0.001||std::fabs(c.q[1].f32[0]-test.wy)>0.001||c.x[0]!=0x123456789||(c.nf!=1||c.zf||c.cf!=1||c.vf)||c.q[2].u64[0]!=0xabcdef)return 1;
  }
  if(BsmCursorHeld(0xdeadbeef00000000)!=0xdeadbeef00000001)return 2;
  _putenv_s("BSM2_TOUCH_CONTROLS","1");
  Arm64Ctx c{};c.q[0].f32[0]=17;BsmCursorTarget(&c);
  if(c.q[0].f32[0]!=17||BsmCursorHeld(0)!=0)return 3;
  _putenv_s("BSM2_TOUCH_CONTROLS","");
  std::puts("cursor mapping: 4 cases; register isolation, control-presence and touch fallback passed");return 0;
}

