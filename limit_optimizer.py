"""Keep exhaustive computed-jump functions out of MSVC's CFG optimizer.

Their conservative switches admit every instruction boundary and cause
quadratic optimizer work. Other functions retain /O2; no guest paths change.
"""
import pathlib,re,json
from project_paths import ROOT, OUT, SOURCE
changes={}
for p in (OUT/'generated').glob('*.c'):
    source=p.read_text()
    def wrap(m):
        body=m.group(0)
        if body.count('case UINT64_C(')<256:return body
        changes[m.group(1)]=body.count('case UINT64_C(')
        return '#if defined(_MSC_VER)\n#pragma optimize("", off)\n#endif\n'+body+'#if defined(_MSC_VER)\n#pragma optimize("", on)\n#endif\n'
    if '#pragma optimize' in source:continue
    result=re.sub(r'void fn0_([0-9a-f]+)\(Arm64Ctx\* c\) \{\n.*?\n\}\n',wrap,source,flags=re.S)
    if result!=source:p.write_text(result)
(OUT/'optimizer_limits.json').write_text(json.dumps(changes,indent=2))
print('Bounded optimizer for',len(changes),'functions')
