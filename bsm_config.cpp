// Optional online configuration can be absent in the desktop environment.
// The native accessor catches this case and selects its own default interval.
// Take that same native fallback without relying on guest C++ unwinding.
#include "arm64_context.h"
#include <cstdint>
#include <cstdio>
#include <cerrno>

extern "C" int BsmProfileSettingsFallback(const Arm64Ctx* c){
  const uint32_t raw=arc_ld32(c->x[0]);
  const uint32_t type=raw^(int32_t(raw)<0?UINT32_MAX:0);
  if(type==0)return 0;
  const int saved=errno;
  fprintf(stderr,"[bsm config] online settings type %u unavailable as Object; using native profile upload interval fallback\n",type);
  errno=saved;
  return 1;
}
