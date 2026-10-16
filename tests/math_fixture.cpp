// Check the host-call ABI, which instruction-level emulation does not cover.
#include "arm64_context.h"
#include "shim.h"
#include <cmath>
#include <cstdio>
static ArcCtxFn resolved;
extern "C" void arc_register_ctx_native(uint64_t,const char*,ArcCtxFn fn){resolved=fn;}
int main(){
  struct Case{const char* name;double a,b,want;bool single;};
  const Case cases[]={{"sin",0,0,0,false},{"cos",0,0,1,false},{"atan2",1,0,1.5707963267948966,false},{"cosf",0,0,1,true},{"sqrtf",4,0,2,true},{"logf",1,0,0,true},{"sinf",0,0,0,true},{"powf",2,3,8,true},{"pow",2,3,8,false}};
  int count=0;
  for(auto& test:cases){resolved=nullptr;if(!arc::ShimResolveMath(test.name)||!resolved)return 1;Arm64Ctx c{};c.x[0]=0x12345678;c.x[1]=0xabcdef;if(test.single){c.q[0].f32[0]=float(test.a);c.q[1].f32[0]=float(test.b);}else{c.q[0].f64[0]=test.a;c.q[1].f64[0]=test.b;}resolved(&c);double got=test.single?c.q[0].f32[0]:c.q[0].f64[0];if(fabs(got-test.want)>1e-6||c.q[0].u64[1]){printf("FAIL %s %.9g expected %.9g\n",test.name,got,test.want);return 2;}++count;}
  arc::ShimResolveMath("difftime");Arm64Ctx time{};time.x[0]=10;time.x[1]=4;resolved(&time);if(time.q[0].f64[0]!=6)return 3;++count;
  arc::ShimResolveMath("strtold_l");Arm64Ctx parsed{};const char* text="1.5 rest";char* end=nullptr;parsed.x[0]=reinterpret_cast<uint64_t>(text);parsed.x[1]=reinterpret_cast<uint64_t>(&end);resolved(&parsed);if(parsed.q[0].u64[0]||parsed.q[0].u64[1]!=UINT64_C(0x3fff800000000000)||end!=text+3)return 4;++count;
  printf("host floating-point ABI cases passed: %d\n",count);return 0;
}
