// Formatting for functions that take the guest's own va_list.
//
// A va_list is not a pointer on this architecture. It is a 32-byte structure
// holding two separate register save areas -- one for integers, one for
// floating point -- plus a cursor into the stack for anything that did not fit
// in registers:
//
//   void* stack;    void* gr_top;    void* vr_top;    int gr_offs;  int vr_offs;
//
// The host's va_list is a flat pointer that walks one contiguous argument
// area. Handing one to the other compiles, links, runs, and formats nonsense:
// every conversion reads from the wrong place, and the *return value* is wrong
// too, which is worse than the visible garbage. A caller that asks vsnprintf
// how long the result would be, allocates that much and copies it, will copy
// the wrong number of bytes.
//
// So the format string is walked here and each argument fetched according to
// the rules the guest was compiled with. Individual conversions are still
// handed to the host's snprintf -- there is no reason to reimplement float
// formatting -- one specifier at a time.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>

std::string BsmWideToUtf8(const uint32_t*);

#include "arm64_context.h"
#include "shim.h"

namespace arc {
namespace {

struct GuestVaList {
  uint64_t stack;
  uint64_t gr_top;
  uint64_t vr_top;
  int32_t gr_offs;
  int32_t vr_offs;
};

// A general-purpose argument: from the register save area while one remains,
// from the stack afterwards. The offsets are negative while registers are left
// and count up towards zero.
uint64_t NextInt(GuestVaList* ap) {
  uint64_t value;
  if (ap->gr_offs < 0) {
    memcpy(&value, reinterpret_cast<void*>(ap->gr_top + ap->gr_offs), 8);
    ap->gr_offs += 8;
  } else {
    memcpy(&value, reinterpret_cast<void*>(ap->stack), 8);
    ap->stack += 8;
  }
  return value;
}

// Floating-point arguments live in their own save area, in 16-byte slots.
double NextDouble(GuestVaList* ap) {
  double value;
  if (ap->vr_offs < 0) {
    memcpy(&value, reinterpret_cast<void*>(ap->vr_top + ap->vr_offs), 8);
    ap->vr_offs += 16;
  } else {
    memcpy(&value, reinterpret_cast<void*>(ap->stack), 8);
    ap->stack += 8;
  }
  return value;
}

// Appends to a caller's buffer, counting what it would have taken even after
// the buffer is full -- which is what the return value has to report.
struct Sink {
  char* out;
  size_t cap;
  size_t used;

  void Put(const char* text, size_t n) {
    if (out && used < cap) {
      const size_t room = cap - used - 1;
      const size_t take = n < room ? n : room;
      if (take) memcpy(out + used, text, take);
    }
    used += n;
  }
};

bool IsConversion(char c) {
  return strchr("diouxXeEfFgGaAcspn%", c) != nullptr;
}

int FormatInto(Sink& sink, const char* fmt, GuestVaList* ap) {
  char spec[64];
  char piece[512];

  for (const char* p = fmt; *p;) {
    if (*p != '%') {
      const char* run = p;
      while (*p && *p != '%') ++p;
      sink.Put(run, static_cast<size_t>(p - run));
      continue;
    }

    const char* start = p++;
    // Flags, width, precision and length, copied through as written. A width
    // or precision given as '*' consumes an argument of its own.
    int stars = 0;
    while (*p && !IsConversion(*p)) {
      if (*p == '*') ++stars;
      ++p;
    }
    if (!*p) break;
    const char conv = *p++;

    if (conv == '%') {
      sink.Put("%", 1);
      continue;
    }

    size_t len = static_cast<size_t>(p - start);
    if (len >= sizeof(spec)) len = sizeof(spec) - 1;
    memcpy(spec, start, len);
    spec[len] = '\0';

    int star_values[2] = {0, 0};
    for (int i = 0; i < stars && i < 2; ++i)
      star_values[i] = static_cast<int>(NextInt(ap));

    int n = 0;
    if (conv == 'f' || conv == 'F' || conv == 'e' || conv == 'E' ||
        conv == 'g' || conv == 'G' || conv == 'a' || conv == 'A') {
      const double v = NextDouble(ap);
      n = stars == 2 ? snprintf(piece, sizeof(piece), spec, star_values[0],
                                star_values[1], v)
          : stars == 1
              ? snprintf(piece, sizeof(piece), spec, star_values[0], v)
              : snprintf(piece, sizeof(piece), spec, v);
    } else if (conv == 's') {
      const char* s = reinterpret_cast<const char*>(NextInt(ap));
      std::string wide;
      if(len>=3&&spec[len-2]=='l'){
        wide=BsmWideToUtf8(reinterpret_cast<const uint32_t*>(s));s=wide.c_str();spec[len-2]='s';spec[len-1]=0;
      }
      if (!s) s = "(null)";
      n = stars == 2 ? snprintf(piece, sizeof(piece), spec, star_values[0],
                                star_values[1], s)
          : stars == 1
              ? snprintf(piece, sizeof(piece), spec, star_values[0], s)
              : snprintf(piece, sizeof(piece), spec, s);
    } else if (conv == 'n') {
      void* target = reinterpret_cast<void*>(NextInt(ap));
      if(target){
        if(strchr(spec,'l')||strchr(spec,'j')||strchr(spec,'z')||strchr(spec,'t')){int64_t n=int64_t(sink.used);memcpy(target,&n,8);}
        else if(strstr(spec,"hh")){int8_t n=int8_t(sink.used);memcpy(target,&n,1);}
        else if(strchr(spec,'h')){int16_t n=int16_t(sink.used);memcpy(target,&n,2);}
        else{int n=int(sink.used);memcpy(target,&n,4);}
      }
      continue;
    } else {
      const uint64_t v = NextInt(ap);
      // Android long is 64-bit; MSVC long is 32-bit. Keep the actual value
      // width for all integer length modifiers before asking the host CRT.
      if(strchr("diouxX",conv)){
        std::string normalized(spec);
        auto length=normalized.find_first_of("ljzt");
        if(length!=std::string::npos){size_t stop=length;while(stop+1<normalized.size()&&strchr("ljzt",normalized[stop]))++stop;normalized.replace(length,stop-length,"ll");strcpy(spec,normalized.c_str());}
      }
      n = stars == 2 ? snprintf(piece, sizeof(piece), spec, star_values[0],
                                star_values[1], v)
          : stars == 1
              ? snprintf(piece, sizeof(piece), spec, star_values[0], v)
              : snprintf(piece, sizeof(piece), spec, v);
    }

    if (n > 0) {
      size_t produced = static_cast<size_t>(n);
      if (produced >= sizeof(piece)) produced = sizeof(piece) - 1;
      sink.Put(piece, produced);
    }
  }

  if (sink.out && sink.cap) {
    const size_t at = sink.used < sink.cap - 1 ? sink.used : sink.cap - 1;
    sink.out[at] = '\0';
  }
  return static_cast<int>(sink.used);
}

// Optional tracing, because "the formatted length is wrong" and "the length is
// right and something downstream is wrong" look identical from a fault
// address. Set ARC_TRACE_FORMAT to tell them apart.
bool Tracing() {
  static const bool on = getenv("ARC_TRACE_FORMAT") != nullptr;
  return on;
}

void TraceResult(const char* who, const char* fmt, int n, const char* text) {
  if (!Tracing()) return;
  fprintf(stderr, "[format] %s -> %d  fmt=\"%s\"", who, n, fmt ? fmt : "(null)");
  if (text) fprintf(stderr, "  out=\"%.120s\"", text);
  fputc('\n', stderr);
}

// --- the entry points the guest actually imports ---------------------------

int Vsnprintf(char* out, uint64_t cap, const char* fmt, GuestVaList* ap) {
  Sink sink{out, static_cast<size_t>(cap), 0};
  const int n = FormatInto(sink, fmt, ap);
  TraceResult("vsnprintf", fmt, n, out);
  return n;
}

int Vsprintf(char* out, const char* fmt, GuestVaList* ap) {
  Sink sink{out, SIZE_MAX, 0};
  return FormatInto(sink, fmt, ap);
}

int VsnprintfChk(char* out, uint64_t cap, int, uint64_t, const char* fmt,
                 GuestVaList* ap) {
  return Vsnprintf(out, cap, fmt, ap);
}

int Vfprintf(void* stream, const char* fmt, GuestVaList* ap) {
  GuestVaList probe = *ap;
  Sink measure{nullptr, 0, 0};
  const int n = FormatInto(measure, fmt, &probe);
  (void)stream;
  // Streams route to stderr the same way the logging shim does; there is no
  // guest FILE* worth honouring here.
  char* buffer = static_cast<char*>(malloc(static_cast<size_t>(n) + 1));
  if (!buffer) return n;
  Sink sink{buffer, static_cast<size_t>(n) + 1, 0};
  FormatInto(sink, fmt, ap);
  fputs(buffer, stderr);
  free(buffer);
  return n;
}

int Vasprintf(char** out, const char* fmt, GuestVaList* ap) {
  GuestVaList probe = *ap;
  Sink measure{nullptr, 0, 0};
  const int n = FormatInto(measure, fmt, &probe);
  *out = static_cast<char*>(malloc(static_cast<size_t>(n) + 1));
  if (!*out) return -1;
  Sink sink{*out, static_cast<size_t>(n) + 1, 0};
  return FormatInto(sink, fmt, ap);
}

int AndroidLogVprint(int prio, const char* tag, const char* fmt,
                     GuestVaList* ap) {
  GuestVaList probe = *ap;
  Sink measure{nullptr, 0, 0};
  const int n = FormatInto(measure, fmt, &probe);
  char* buffer = static_cast<char*>(malloc(static_cast<size_t>(n) + 1));
  if (!buffer) return n;
  Sink sink{buffer, static_cast<size_t>(n) + 1, 0};
  FormatInto(sink, fmt, ap);
  fprintf(stderr, "[%d] %s: %s\n", prio, tag ? tag : "?", buffer);
  free(buffer);
  return n;
}

// --- variadic calls --------------------------------------------------------
//
// A variadic call and a va_list describe the same thing. The named arguments
// take general registers in order; everything after them continues through the
// remaining general registers, with floating-point arguments taking vector
// registers instead, and the overflow going to the stack. That is exactly what
// a va_list records -- so one can be synthesised from the register state and
// handed to the same formatter.
//
// `named_gp` is how many general registers the named parameters used, which is
// the only thing that differs between these functions.
GuestVaList VaFromContext(Arm64Ctx* c, int named_gp) {
  GuestVaList v;
  v.stack = c->sp;
  v.gr_top = reinterpret_cast<uint64_t>(&c->x[8]);
  v.vr_top = reinterpret_cast<uint64_t>(&c->q[8]);
  v.gr_offs = -static_cast<int32_t>((8 - named_gp) * 8);
  v.vr_offs = -static_cast<int32_t>(8 * 16);
  return v;
}

// Formats into a fresh buffer the caller frees.
char* FormatAlloc(const char* fmt, GuestVaList* ap, int* length) {
  GuestVaList measure = *ap;
  Sink probe{nullptr, 0, 0};
  const int n = FormatInto(probe, fmt, &measure);
  char* buffer = static_cast<char*>(malloc(static_cast<size_t>(n) + 1));
  if (!buffer) {
    *length = 0;
    return nullptr;
  }
  Sink sink{buffer, static_cast<size_t>(n) + 1, 0};
  FormatInto(sink, fmt, ap);
  *length = n;
  return buffer;
}

void PrintfCtx(Arm64Ctx* c) {
  GuestVaList ap = VaFromContext(c, 1);
  int n = 0;
  char* text = FormatAlloc(reinterpret_cast<const char*>(c->x[0]), &ap, &n);
  if (text) {
    fputs(text, stdout);
    free(text);
  }
  c->x[0] = static_cast<uint64_t>(n);
}

void FprintfCtx(Arm64Ctx* c) {
  GuestVaList ap = VaFromContext(c, 2);
  int n = 0;
  char* text = FormatAlloc(reinterpret_cast<const char*>(c->x[1]), &ap, &n);
  if (text) {
    fputs(text, stderr);
    free(text);
  }
  c->x[0] = static_cast<uint64_t>(n);
}

void SprintfCtx(Arm64Ctx* c) {
  GuestVaList ap = VaFromContext(c, 2);
  Sink sink{reinterpret_cast<char*>(c->x[0]), SIZE_MAX, 0};
  c->x[0] = static_cast<uint64_t>(
      FormatInto(sink, reinterpret_cast<const char*>(c->x[1]), &ap));
}

void SnprintfCtx(Arm64Ctx* c) {
  GuestVaList ap = VaFromContext(c, 3);
  char* out = reinterpret_cast<char*>(c->x[0]);
  const char* fmt = reinterpret_cast<const char*>(c->x[2]);
  Sink sink{out, static_cast<size_t>(c->x[1]), 0};
  const int n = FormatInto(sink, fmt, &ap);
  TraceResult("snprintf", fmt, n, out);
  c->x[0] = static_cast<uint64_t>(n);
}

void AsprintfCtx(Arm64Ctx* c) {
  GuestVaList ap = VaFromContext(c, 2);
  int n = 0;
  char* text = FormatAlloc(reinterpret_cast<const char*>(c->x[1]), &ap, &n);
  *reinterpret_cast<char**>(c->x[0]) = text;
  c->x[0] = text ? static_cast<uint64_t>(n) : UINT64_C(0xFFFFFFFFFFFFFFFF);
}

void SyslogCtx(Arm64Ctx* c) {
  GuestVaList ap = VaFromContext(c, 2);
  int n = 0;
  char* text = FormatAlloc(reinterpret_cast<const char*>(c->x[1]), &ap, &n);
  if (text) {
    fprintf(stderr, "%s\n", text);
    free(text);
  }
  c->x[0] = 0;
}

// Not variadic -- these take the context for the other reason it exists. Their
// first argument is a float, which lives in v0, and the thunk carries only the
// integer registers. Bound the ordinary way, the host function receives the two
// output pointers shifted into the wrong parameter slots and writes through
// whatever the guest happened to leave in x2.
//
// That is not hypothetical: it is what three of The Simpsons' static
// constructors were faulting on, writing to address 0x16 because 0x16 is what
// x2 held. The comment on the thunk has always said to generate one of these
// when a title needs it. One did.
void SincosfCtx(Arm64Ctx* c) {
  const float x = c->q[0].f32[0];
  if (c->x[0]) *reinterpret_cast<float*>(c->x[0]) = sinf(x);
  if (c->x[1]) *reinterpret_cast<float*>(c->x[1]) = cosf(x);
}

void SincosCtx(Arm64Ctx* c) {
  const double x = c->q[0].f64[0];
  if (c->x[0]) *reinterpret_cast<double*>(c->x[0]) = sin(x);
  if (c->x[1]) *reinterpret_cast<double*>(c->x[1]) = cos(x);
}

void AndroidLogPrintCtx(Arm64Ctx* c) {
  GuestVaList ap = VaFromContext(c, 3);
  int n = 0;
  char* text = FormatAlloc(reinterpret_cast<const char*>(c->x[2]), &ap, &n);
  fprintf(stderr, "[%d] %s: %s\n", static_cast<int>(c->x[0]),
          c->x[1] ? reinterpret_cast<const char*>(c->x[1]) : "?",
          text ? text : "");
  free(text);
  c->x[0] = static_cast<uint64_t>(n);
}

struct CtxEntry {
  const char* name;
  ArcCtxFn fn;
};
int Vsscanf(const char*,const char*,GuestVaList*);
void SscanfCtx(Arm64Ctx* c){auto ap=VaFromContext(c,2);c->x[0]=uint64_t(Vsscanf(reinterpret_cast<const char*>(c->x[0]),reinterpret_cast<const char*>(c->x[1]),&ap));}

const CtxEntry kCtxTable[] = {
    {"sscanf", SscanfCtx},
    {"printf", PrintfCtx},
    {"fprintf", FprintfCtx},
    {"sprintf", SprintfCtx},
    {"snprintf", SnprintfCtx},
    {"asprintf", AsprintfCtx},
    {"syslog", SyslogCtx},
    {"__android_log_print", AndroidLogPrintCtx},
    {"sincosf", SincosfCtx},
    {"sincos", SincosCtx},
};

struct Entry {
  const char* name;
  void* fn;
};

int Vsscanf(const char* text,const char* fmt,GuestVaList* input) {
  // Convert only integer length modifiers; %lf still means double.
  std::string normalized;
  for(const char* p=fmt;*p;){if(*p!='%'){normalized+=*p++;continue;}const char* start=p++;if(*p=='%'){normalized.append(start,2);++p;continue;}while(*p&&strchr("*0123456789",*p))++p;const char* length=p;while(*p&&strchr("hljztL",*p))++p;const char conv=*p;if(!conv){normalized+=start;break;}normalized.append(start,length-start);if(strchr("diouxXn",conv)&&length<p&&strchr("ljzt",*length))normalized+="ll";else normalized.append(length,p-length);normalized+=*p++;if(conv=='['){if(*p=='^')normalized+=*p++;if(*p==']')normalized+=*p++;while(*p&&*p!=']')normalized+=*p++;if(*p)normalized+=*p++;}}
  fmt=normalized.c_str();
  GuestVaList ap=*input;void* args[16]={};int count=0;
  for(const char* p=fmt;*p;++p){if(*p!='%')continue;++p;if(*p=='%')continue;bool suppressed=*p=='*';if(suppressed)++p;while(*p&&strchr("0123456789hljztL",*p))++p;if(!suppressed){if(count==16)arc_trap(nullptr,"vsscanf exceeds 16 output arguments");args[count++]=reinterpret_cast<void*>(NextInt(&ap));}if(*p=='['){++p;if(*p=='^')++p;if(*p==']')++p;while(*p&&*p!=']')++p;}if(!*p)break;}
  int result=sscanf(text,fmt,args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15]);
  static int left=12;if(left-->0)fprintf(stderr,"[bsm vsscanf] %s -> %d from %.80s\n",fmt,result,text);return result;
}

#define E(sym, fn) {sym, reinterpret_cast<void*>(&fn)}
const Entry kTable[] = {
    E("vsscanf", Vsscanf),
    E("vsnprintf", Vsnprintf),
    E("vsprintf", Vsprintf),
    E("__vsnprintf_chk", VsnprintfChk),
    E("vfprintf", Vfprintf),
    E("vasprintf", Vasprintf),
    E("__android_log_vprint", AndroidLogVprint),
};
#undef E

}  // namespace

// One integer-class argument out of a guest va_list, for callers outside this
// file. The JNI bridge needs it: a Java method invoked through Call*MethodV is
// handed its arguments this way, and the engine's own log goes through one of
// them -- so without this, the log the engine writes about itself is dropped
// on the floor.
//
// `cursor` is the caller's own copy of the va_list, advanced in place, so a
// caller can walk several arguments without disturbing the guest's.
uint64_t ShimVaNextInt(void* cursor) {
  return NextInt(static_cast<GuestVaList*>(cursor));
}
double ShimVaNextDouble(void* cursor) { return NextDouble(static_cast<GuestVaList*>(cursor)); }

size_t ShimVaListSize() { return sizeof(GuestVaList); }

uint64_t ShimResolveVarargs(const char* name) {
  for (const Entry& e : kTable)
    if (strcmp(e.name, name) == 0) return reinterpret_cast<uint64_t>(e.fn);
  // A variadic function resolves to its own handler's address, which the
  // dispatcher then recognises as wanting the context rather than eight
  // integers.
  for (const CtxEntry& e : kCtxTable)
    if (strcmp(e.name, name) == 0) return reinterpret_cast<uint64_t>(e.fn);
  return 0;
}

void ShimRegisterVarargs() {
  for (const CtxEntry& e : kCtxTable)
    arc_register_ctx_native(reinterpret_cast<uint64_t>(e.fn), e.name, e.fn);
}

}  // namespace arc
