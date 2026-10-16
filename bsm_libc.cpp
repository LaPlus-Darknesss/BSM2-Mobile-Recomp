// Android LP64 interfaces whose Windows CRT equivalents have different ABIs.
#include "arm64_context.h"
#include "shim.h"
#include <windows.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <cstdio>
#include <string>
#include <algorithm>
std::string BsmWideToUtf8(const uint32_t*);

namespace {
int64_t Strtol(const char* s,char** e,int b){return _strtoi64(s,e,b);}
uint64_t Strtoul(const char* s,char** e,int b){return _strtoui64(s,e,b);}
int64_t Clock(){
  FILETIME created,exited,kernel,user;
  if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))return -1;
  auto ticks=[](FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
  return int64_t((ticks(kernel)+ticks(user))/10); // Android: CPU microseconds.
}
int Rename(const char* from,const char* to){
  if(MoveFileExA(from,to,MOVEFILE_REPLACE_EXISTING))return 0;
  const auto error=GetLastError();errno=error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND?ENOENT:error==ERROR_NOT_SAME_DEVICE?EXDEV:EACCES;return -1;
}
int64_t Ftell(FILE* f){return _ftelli64(f);}
int Fseek(FILE* f,int64_t offset,int whence){return _fseeki64(f,offset,whence);}
struct GuestTm {int sec,min,hour,mday,mon,year,wday,yday,isdst,pad;int64_t gmtoff;const char* zone;};
static_assert(sizeof(GuestTm)==56);
tm HostTm(const GuestTm* g){tm t{};t.tm_sec=g->sec;t.tm_min=g->min;t.tm_hour=g->hour;t.tm_mday=g->mday;t.tm_mon=g->mon;t.tm_year=g->year;t.tm_wday=g->wday;t.tm_yday=g->yday;t.tm_isdst=g->isdst;return t;}
void StoreTm(GuestTm* g,const tm& t){g->sec=t.tm_sec;g->min=t.tm_min;g->hour=t.tm_hour;g->mday=t.tm_mday;g->mon=t.tm_mon;g->year=t.tm_year;g->wday=t.tm_wday;g->yday=t.tm_yday;g->isdst=t.tm_isdst;g->pad=0;long west=0,bias=0;_get_timezone(&west);_get_dstbias(&bias);g->gmtoff=-int64_t(west+(t.tm_isdst>0?bias:0));static thread_local char zone[128];if(!strftime(zone,sizeof zone,"%Z",&t))strcpy(zone,"UTC");g->zone=zone;}
int64_t Mktime(GuestTm* g){tm t=HostTm(g);const auto result=_mktime64(&t);if(result!=-1)StoreTm(g,t);return result;}
size_t Strftime(char* d,size_t n,const char* fmt,const GuestTm* g,void* =nullptr){tm t=HostTm(g);return strftime(d,n,fmt,&t);}
using W=uint32_t;
size_t Wlen(const W* s){const W* p=s;while(*p)++p;return p-s;}
W* Wcopy(W* d,const W* s,size_t n){memcpy(d,s,n*4);return d;}
W* Wmove(W* d,const W* s,size_t n){memmove(d,s,n*4);return d;}
W* Wset(W* d,W c,size_t n){std::fill_n(d,n,c);return d;}
const W* Wchr(const W* s,W c,size_t n){for(size_t i=0;i<n;++i)if(s[i]==c)return s+i;return nullptr;}
int Wcmp(const W* a,const W* b,size_t n){for(size_t i=0;i<n;++i)if(a[i]!=b[i])return a[i]<b[i]?-1:1;return 0;}
int Wcoll(const W* a,const W* b,void*){for(;*a&&*a==*b;++a,++b){}return *a==*b?0:*a<*b?-1:1;}
size_t Wxfrm(W* d,const W* s,size_t n,void*){size_t size=Wlen(s);if(d)Wcopy(d,s,std::min(n,size+1));return size;}
std::string NarrowNumber(const W* s){std::string text;for(;*s;++s)text.push_back(*s<128?char(*s):'\x7f');return text;}
int64_t Wtol(const W* s,const W** e,int b){auto text=NarrowNumber(s);char* end;auto n=Strtol(text.c_str(),&end,b);if(e)*e=s+(end-text.c_str());return n;}
uint64_t Wtoul(const W* s,const W** e,int b){auto text=NarrowNumber(s);char* end;auto n=Strtoul(text.c_str(),&end,b);if(e)*e=s+(end-text.c_str());return n;}
void WideFloat(Arm64Ctx* c,const char* kind){const W* s=reinterpret_cast<const W*>(c->x[0]);auto text=NarrowNumber(s);char* end=nullptr;auto out=reinterpret_cast<const W**>(c->x[1]);c->x[0]=reinterpret_cast<uint64_t>(text.c_str());c->x[1]=reinterpret_cast<uint64_t>(&end);auto fn=reinterpret_cast<ArcCtxFn>(arc::ShimResolveMath(kind));fn(c);if(out)*out=s+(end-text.c_str());}
void Wtod(Arm64Ctx* c){WideFloat(c,"strtod");}
void Wtof(Arm64Ctx* c){WideFloat(c,"strtof");}
void Wtold(Arm64Ctx* c){WideFloat(c,"strtold");}
// Keep our opaque UTF-8 decoder state in one 32-bit word, without host wchar_t.
size_t Decode(W* out,const char* s,size_t n,uint32_t* ps){
  static thread_local uint32_t fallback=0;if(!ps)ps=&fallback;
  if(!s){*ps=0;return 0;}if(!n)return size_t(-2);
  uint32_t value=*ps&0x1fffff;unsigned need=(*ps>>24)&7,total=(*ps>>28)&7;size_t i=0;
  if(!need){unsigned c=uint8_t(s[i++]);if(c<128){if(out)*out=c;*ps=0;return c?1:0;}
    if(c>=0xc2&&c<=0xdf){need=1;total=2;value=c&31;}else if(c>=0xe0&&c<=0xef){need=2;total=3;value=c&15;}else if(c>=0xf0&&c<=0xf4){need=3;total=4;value=c&7;}else{errno=EILSEQ;*ps=0;return size_t(-1);}}
  while(need&&i<n){unsigned c=uint8_t(s[i++]);if((c&0xc0)!=0x80){errno=EILSEQ;*ps=0;return size_t(-1);}value=(value<<6)|(c&63);--need;}
  if(need){*ps=value|(need<<24)|(total<<28);return size_t(-2);}
  *ps=0;if(value>0x10ffff||(value>=0xd800&&value<=0xdfff)||(total==2&&value<0x80)||(total==3&&value<0x800)||(total==4&&value<0x10000)){errno=EILSEQ;return size_t(-1);}if(out)*out=value;return i;
}
size_t Mbrlen(const char* s,size_t n,uint32_t* state){return Decode(nullptr,s,n,state);}
int Mbtowc(W* d,const char* s,size_t n){uint32_t state=0;auto r=Decode(d,s,n,&state);return r==size_t(-2)?-1:int(r);}
W Btowc(int c){return c>=0&&c<128?W(c):UINT32_MAX;}
int Wctob(W c){return c<128?int(c):-1;}
size_t Encode(char* d,W c,uint32_t* state){if(state)*state=0;if(!d)return 1;if(c<128){d[0]=char(c);return 1;}if(c>0x10ffff||(c>=0xd800&&c<=0xdfff)){errno=EILSEQ;return size_t(-1);}unsigned n=c<0x800?2:c<0x10000?3:4;for(unsigned i=n-1;i;--i){d[i]=char(0x80|(c&63));c>>=6;}d[0]=char((n==2?0xc0:n==3?0xe0:0xf0)|c);return n;}
size_t Mbsn(W* d,const char** input,size_t bytes,size_t len,uint32_t* ps){
  uint32_t copy=ps?*ps:0;const char* s=*input;size_t count=0;
  while(bytes&&(!d||count<len)){W c;auto n=Decode(&c,s,bytes,&copy);if(n==size_t(-1)){if(d){*input=s;if(ps)*ps=copy;}return n;}if(n==size_t(-2)){s+=bytes;bytes=0;break;}if(!n){if(d){d[count]=0;*input=nullptr;if(ps)*ps=copy;}return count;}if(d)d[count]=c;++count;s+=n;bytes-=n;}
  if(d){*input=s;if(ps)*ps=copy;}return count;
}
size_t Mbs(W* d,const char** input,size_t n,uint32_t* ps){return Mbsn(d,input,SIZE_MAX,n,ps);}
size_t Wcsn(char* d,const W** input,size_t count,size_t len,uint32_t* ps){const W* s=*input;size_t used=0;while(count--){char bytes[4];auto n=Encode(bytes,*s,nullptr);if(n==size_t(-1)){if(d)*input=s;return n;}if(d&&n>len-used)break;if(d)memcpy(d+used,bytes,n);if(!*s){if(d){*input=nullptr;if(ps)*ps=0;}return used;}used+=n;++s;}if(d){*input=s;if(ps)*ps=0;}return used;}
size_t MbMax(){return 4;}
void Swprintf(Arm64Ctx* c){
  W* destination=reinterpret_cast<W*>(c->x[0]);size_t capacity=size_t(c->x[1]);
  auto format=BsmWideToUtf8(reinterpret_cast<const W*>(c->x[2]));
  if(!capacity||capacity>(SIZE_MAX-1)/4){c->x[0]=uint64_t(-1);return;}
  std::string bytes(capacity*4+1,0);
  c->x[0]=reinterpret_cast<uint64_t>(bytes.data());c->x[1]=bytes.size();c->x[2]=reinterpret_cast<uint64_t>(format.c_str());
  auto fn=reinterpret_cast<ArcCtxFn>(arc::ShimResolveVarargs("snprintf"));fn(c);
  if(int64_t(c->x[0])<0||c->x[0]>=bytes.size()){destination[0]=0;c->x[0]=uint64_t(-1);return;}
  size_t pos=0,count=0;uint32_t state=0;
  while(bytes[pos]){W value;auto n=Decode(&value,bytes.c_str()+pos,bytes.size()-pos,&state);if(n>=size_t(-2)||count+1>=capacity){destination[std::min(count,capacity-1)]=0;c->x[0]=uint64_t(-1);return;}destination[count++]=value;pos+=n;}
  destination[count]=0;c->x[0]=count;
}
struct Entry {const char* name;uint64_t fn;};
#define E(n,f) {n,reinterpret_cast<uint64_t>(&f)}
Entry entries[]={E("strtol",Strtol),E("strtoul",Strtoul),E("clock",Clock),E("rename",Rename),E("fseek",Fseek),E("ftell",Ftell),E("mktime",Mktime),E("strftime",Strftime),E("strftime_l",Strftime),E("wcslen",Wlen),E("wmemcpy",Wcopy),E("wmemmove",Wmove),E("wmemset",Wset),E("wmemchr",Wchr),E("wmemcmp",Wcmp),E("wcscoll_l",Wcoll),E("wcsxfrm_l",Wxfrm),E("wcstol",Wtol),E("wcstoll",Wtol),E("wcstoul",Wtoul),E("wcstoull",Wtoul),E("mbrtowc",Decode),E("mbrlen",Mbrlen),E("mbtowc",Mbtowc),E("btowc",Btowc),E("wctob",Wctob),E("wcrtomb",Encode),E("mbsnrtowcs",Mbsn),E("mbsrtowcs",Mbs),E("wcsnrtombs",Wcsn),E("__ctype_get_mb_cur_max",MbMax)};
#undef E
}
uint64_t BsmResolveLibc(const char* name){
  for(auto e:entries)if(!strcmp(e.name,name)){arc_register_native(e.fn,name);return e.fn;}
  ArcCtxFn ctx=nullptr;if(!strcmp(name,"wcstod"))ctx=Wtod;else if(!strcmp(name,"wcstof"))ctx=Wtof;else if(!strcmp(name,"wcstold"))ctx=Wtold;else if(!strcmp(name,"swprintf"))ctx=Swprintf;
  if(ctx){auto a=reinterpret_cast<uint64_t>(ctx);arc_register_ctx_native(a,name,ctx);return a;}return 0;
}
std::string BsmWideToUtf8(const uint32_t* s){std::string result;if(!s)return "(null)";for(;*s;++s){char bytes[4];auto n=Encode(bytes,*s,nullptr);if(n==size_t(-1))return result;result.append(bytes,n);}return result;}
