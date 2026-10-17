"""Apply the version-pinned movement adapter to local generated code."""
import hashlib,pathlib
from project_paths import ROOT, OUT, SOURCE
ENGINE=OUT/'run/libnative.so'
def install():
    expected='1b3e1e45589c10945a5941949552136647c40842cd96d437fd96dfce2a4387b9'
    if hashlib.sha256(ENGINE.read_bytes()).hexdigest()!=expected:raise RuntimeError('Cursor adapter requires the pinned Android 1.10 engine')
    from install_config_hook import install as install_config
    install_config()
    probe=OUT/'generated/lifted_043.c'
    diagnostic=probe.read_text()
    entry='void fn0_aa2728(Arm64Ctx* c) {'
    if 'BsmTraceThrow(c);' not in diagnostic:
        if diagnostic.count(entry)!=1:raise RuntimeError('Pinned exception entry is missing')
        diagnostic=diagnostic.replace('#include "lifted.h"\n','#include "lifted.h"\nvoid BsmTraceThrow(Arm64Ctx* c);\n',1).replace(entry,entry+'\n  BsmTraceThrow(c);',1)
        probe.write_text(diagnostic)
    probe=OUT/'generated/lifted_047.c'
    diagnostic=probe.read_text()
    entry='L_390a08:;'
    if 'BsmTraceJsonType(c);' not in diagnostic:
        if diagnostic.count(entry)!=1:raise RuntimeError('Pinned JSON mismatch branch is missing')
        diagnostic=diagnostic.replace('#include "lifted.h"\n','#include "lifted.h"\nvoid BsmTraceJsonType(Arm64Ctx* c);\n',1).replace(entry,entry+'\n  BsmTraceJsonType(c);',1)
        probe.write_text(diagnostic)
    source=OUT/'generated/lifted_059.c'
    text=source.read_text()
    declaration='void BsmCursorTarget(Arm64Ctx* c);\n'
    hook='void fn0_4ade5c(Arm64Ctx* c) {\n  BsmCursorTarget(c);'
    original='void fn0_4ade5c(Arm64Ctx* c) {'
    if hook not in text:
        if text.count(original)!=1:raise RuntimeError('Movement target entry is missing or ambiguous')
        text=text.replace('#include "lifted.h"\n','#include "lifted.h"\n'+declaration,1).replace(original,hook,1)
        source.write_text(text)
    source=OUT/'generated/lifted_009.c'
    text=source.read_text()
    if 'uint64_t BsmCursorHeld(uint64_t native);' in text:return
    begin=text.index('void fn0_55db30(Arm64Ctx* c) {')
    end=text.find('\nvoid fn0_',begin+1)
    if end<0:end=len(text)
    body=text[begin:end]
    import re
    # Only the gameplay Process method's two control-presence reads. UI touch
    # processing remains original; never write the screen's real finger flag.
    pattern=r'arc_ld(8|64)\(\(ARC_X_R\(c, 19\) \+ INT64_C\(1432\)\)\)'
    body,count=re.subn(pattern,lambda m:'BsmCursorHeld('+m.group(0)+')',body)
    if count!=2:raise RuntimeError(f'Expected two gameplay control-presence reads, got {count}')
    text=text[:begin]+body+text[end:]
    source.write_text(text.replace('#include "lifted.h"\n','#include "lifted.h"\nuint64_t BsmCursorHeld(uint64_t native);\n',1))
if __name__=='__main__':install()
