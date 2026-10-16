"""Lift explicitly audited callback entries omitted by unwind discovery."""
import pathlib,sys,re,json
from project_paths import ROOT, OUT, SOURCE
GEN=OUT/'generated'

sys.path.insert(0,str(SOURCE/'vendor/androidrecomp/tools'))
from elftools.elf.elffile import ELFFile
import lifter
entries_path=OUT/'extra_entries.json';entries=json.loads(entries_path.read_text()) if entries_path.exists() else {}
for arg in sys.argv[1:]:
    a,n=arg.split(':');entries[a]=int(n,16)
with (OUT/'run/libnative.so').open('rb') as f:
    elf=ELFFile(f);l=lifter.Lifter();l.image_sections=lifter.data_sections(elf);l.plt_names=lifter.plt_symbols(elf);sections=lifter.code_sections(elf);bodies=[]
    for a,n in sorted(entries.items(),key=lambda x:int(x[0],16)):
        body=l.lift_function(int(a,16),n,lifter.bytes_at(sections,int(a,16),n));assert body,str(l.unsupported);bodies.append(body)
(GEN/'extra_entries.c').write_text('#include "lifted.h"\n'+''.join('void fn0_'+a+'(Arm64Ctx*);\n' for a in entries)+''.join(bodies))
dispatch=(GEN/'dispatch.c').read_text();pattern=r'static const ArcEntry kImage0\[\] = \{\n(.*?)\n\};';m=re.search(pattern,dispatch,re.S);assert m
items={int(a):name for a,name in re.findall(r'\{ UINT64_C\((\d+)\), (fn0_[0-9a-f]+) \}',m.group(1))}
for a in entries:items[int(a,16)]='fn0_'+a
table='static const ArcEntry kImage0[] = {\n'+'\n'.join('  { UINT64_C('+str(a)+'), '+name+' },' for a,name in sorted(items.items()))+'\n};'
dispatch=dispatch[:m.start()]+table+dispatch[m.end():];dispatch=re.sub(r'\{ kImage0, \d+ \}', '{ kImage0, '+str(len(items))+' }',dispatch)
for a in entries:
    declaration='void fn0_'+a+'(Arm64Ctx*);'
    if declaration not in dispatch:dispatch=dispatch.replace('#include "lifted.h"','#include "lifted.h"\n'+declaration)
(GEN/'dispatch.c').write_text(dispatch);entries_path.write_text(json.dumps(entries,indent=2));print('Audited entry count',len(entries))
