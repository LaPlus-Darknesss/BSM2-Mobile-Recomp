#include "arm64_context.h"
#include "shim.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <windows.h>
uint64_t BsmResolveLibc(const char*);
extern "C" void arc_register_native(uint64_t,const char*){}
extern "C" void arc_register_ctx_native(uint64_t,const char*,ArcCtxFn){}
extern "C" void arc_trap(Arm64Ctx*,const char* message){fprintf(stderr,"%s\n",message);abort();}
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
template<class T>T Native(const char* name){return reinterpret_cast<T>(BsmResolveLibc(name));}
int main(){int checks=0;
  auto integer=Native<int64_t(*)(const char*,char**,int)>("strtol");auto unsign=Native<uint64_t(*)(const char*,char**,int)>("strtoul");
  char* end;const char* number="-4294967297 trailing";CHECK(integer(number,&end,10)==-4294967297LL&&end==number+11);
  CHECK(unsign("18446744073709551615",nullptr,10)==UINT64_MAX);
  errno=0;CHECK(integer("9223372036854775808",nullptr,10)==INT64_MAX&&errno==ERANGE);
  uint32_t words[]={0x41,0x1f680,0x42,0},destination[6]={9,9,9,9,0xaabbccdd,0x11223344};
  CHECK(Native<size_t(*)(const uint32_t*)>("wcslen")(words)==3);
  Native<uint32_t*(*)(uint32_t*,const uint32_t*,size_t)>("wmemcpy")(destination,words,4);
  CHECK(!memcmp(destination,words,16)&&destination[4]==0xaabbccdd);
  Native<uint32_t*(*)(uint32_t*,const uint32_t*,size_t)>("wmemmove")(destination+1,destination,3);CHECK(destination[2]==0x1f680&&destination[3]==0x42);
  uint32_t state=0,code=0;auto decode=Native<size_t(*)(uint32_t*,const char*,size_t,uint32_t*)>("mbrtowc");
  CHECK(decode(&code,"\xf0\x9f",2,&state)==size_t(-2)&&state);
  CHECK(decode(&code,"\x9a\x80",2,&state)==2&&code==0x1f680&&!state);
  CHECK(decode(&code,"\xc0\x80",2,&state)==size_t(-1));
  CHECK(decode(&code,"\xed\xa0\x80",3,&state)==size_t(-1));
  char utf8[5]{};CHECK(Native<size_t(*)(char*,uint32_t,uint32_t*)>("wcrtomb")(utf8,0x1f680,&state)==4&&!memcmp(utf8,"\xf0\x9f\x9a\x80",4));
  const char* input="A\xf0\x9f\x9a\x80";uint32_t output[4]={};
  CHECK(Native<size_t(*)(uint32_t*,const char**,size_t,uint32_t*)>("mbsrtowcs")(output,&input,4,&state)==2&&input==nullptr&&output[1]==0x1f680&&output[2]==0);
  const uint32_t wideNumber[]={'4','2','9','4','9','6','7','2','9','7','x',0};const uint32_t* wideEnd;
  CHECK(Native<int64_t(*)(const uint32_t*,const uint32_t**,int)>("wcstol")(wideNumber,&wideEnd,10)==4294967297LL&&wideEnd==wideNumber+10);
  uint32_t fraction[]={'1','.','5','x',0};Arm64Ctx c{};c.x[0]=reinterpret_cast<uint64_t>(fraction);c.x[1]=reinterpret_cast<uint64_t>(&wideEnd);reinterpret_cast<ArcCtxFn>(BsmResolveLibc("wcstold"))(&c);CHECK(c.q[0].u64[1]==UINT64_C(0x3fff800000000000)&&wideEnd==fraction+3);
  char formatted[200];int64_t written=-1;c={};c.x[0]=reinterpret_cast<uint64_t>(formatted);c.x[1]=sizeof formatted;c.x[2]=reinterpret_cast<uint64_t>("%ld/%lu/%ls%ln");c.x[3]=uint64_t(-4294967297LL);c.x[4]=UINT64_MAX;c.x[5]=reinterpret_cast<uint64_t>(words);c.x[6]=reinterpret_cast<uint64_t>(&written);
  reinterpret_cast<ArcCtxFn>(arc::ShimResolveVarargs("snprintf"))(&c);
  CHECK(!strcmp(formatted,"-4294967297/18446744073709551615/A\xf0\x9f\x9a\x80" "B")&&written==int64_t(strlen(formatted)));
  uint32_t format[]={'%','l','d',0},wideOut[30]{};c={};c.x[0]=reinterpret_cast<uint64_t>(wideOut);c.x[1]=30;c.x[2]=reinterpret_cast<uint64_t>(format);c.x[3]=4294967297LL;reinterpret_cast<ArcCtxFn>(BsmResolveLibc("swprintf"))(&c);CHECK(c.x[0]==10&&wideOut[0]=='4'&&wideOut[9]=='7'&&wideOut[10]==0);
  int64_t scanned=0;c={};c.x[0]=reinterpret_cast<uint64_t>("-4294967297");c.x[1]=reinterpret_cast<uint64_t>("%ld");c.x[2]=reinterpret_cast<uint64_t>(&scanned);reinterpret_cast<ArcCtxFn>(arc::ShimResolveVarargs("sscanf"))(&c);CHECK(c.x[0]==1&&scanned==-4294967297LL);
  FILE* file=tmpfile();CHECK(file!=nullptr);CHECK(Native<int(*)(FILE*,int64_t,int)>("fseek")(file,INT64_C(0x100000001),SEEK_SET)==0);CHECK(Native<int64_t(*)(FILE*)>("ftell")(file)==INT64_C(0x100000001));fclose(file);
  struct GuestTm{int sec,min,hour,day,month,year,wday,yday,dst,pad;int64_t offset;const char* zone;};GuestTm date{};date.day=2;date.month=0;date.year=124;date.dst=-1;CHECK(Native<int64_t(*)(GuestTm*)>("mktime")(&date)>0&&date.zone);
  CHECK(Native<size_t(*)(char*,size_t,const char*,const GuestTm*)>("strftime")(formatted,sizeof formatted,"%Y-%m-%d",&date)==10&&!strcmp(formatted,"2024-01-02"));
  char temp[MAX_PATH];GetTempPathA(MAX_PATH,temp);char a[MAX_PATH],b[MAX_PATH];GetTempFileNameA(temp,"bsm",0,a);GetTempFileNameA(temp,"bsm",0,b);FILE* f=fopen(a,"wb");fputs("new",f);fclose(f);CHECK(Native<int(*)(const char*,const char*)>("rename")(a,b)==0);f=fopen(b,"rb");CHECK(f&&fgetc(f)=='n');fclose(f);remove(b);
  auto clock=Native<int64_t(*)()>("clock");FILETIME created,exited,kernel,user;GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user);uint64_t expected=(((uint64_t(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime)+((uint64_t(user.dwHighDateTime)<<32)|user.dwLowDateTime))/10;CHECK(clock()>=int64_t(expected)&&clock()-int64_t(expected)<100000);
  printf("Android/Windows libc ABI checks passed: %d (LP64, UTF-32/UTF-8, formats, time, large seek, replace)\n",checks);
}
