"""Preserve the native optional upload-setting fallback without guest unwinding."""
import hashlib
from project_paths import OUT

def install():
    expected='1b3e1e45589c10945a5941949552136647c40842cd96d437fd96dfce2a4387b9'
    if hashlib.sha256((OUT/'run/libnative.so').read_bytes()).hexdigest()!=expected:
        raise RuntimeError('Configuration adapter requires the pinned Android 1.10 engine')
    source=OUT/'generated/lifted_078.c'
    text=source.read_text()
    marker='  if (BsmProfileSettingsFallback(c)) {'
    if marker in text:return
    begin=text.index('void fn0_468280(Arm64Ctx* c) {')
    end=text.index('\nvoid fn0_',begin+1)
    body=text[begin:end]
    needle='  fn0_3909ac(c);'
    if body.count(needle)!=1 or body.count('L_468448:;')!=1:
        raise RuntimeError('Pinned profile interval fallback is missing or ambiguous')
    guard='''  if (BsmProfileSettingsFallback(c)) {
    /* Match native fallback state before original JSON destruction/epilogue. */
    ARC_X_W(c, 23, ARC_X_R(c, 0));
    ARC_X_W(c, 19, ARC_X_R(c, 0) + UINT64_C(8));
    goto L_468448;
  }
'''
    body=body.replace(needle,guard+needle,1)
    text=text[:begin]+body+text[end:]
    text=text.replace('#include "lifted.h"\n','#include "lifted.h"\nint BsmProfileSettingsFallback(const Arm64Ctx* c);\n',1)
    source.write_text(text)

if __name__=='__main__':install()
