"""Re-lift ranges rejected only because their trailing literal data decodes as UDF."""
import pathlib,sys,re,io,json,bisect
from project_paths import ROOT, OUT, SOURCE
GEN=OUT/'generated'

sys.path.insert(0,str(SOURCE/'vendor/androidrecomp/tools'))
from elftools.elf.elffile import ELFFile
import lifter
stubpath=GEN/'stubs.c';original=stubpath.read_text();backup=OUT/'original_stubs.c'
if not backup.exists():backup.write_text(original)
source=backup.read_text();addresses=[int(a,16) for a in re.findall(r'void fn0_([0-9a-f]+)',source)]
starts=sorted(set(int(a,16) for a in re.findall(r'void fn0_([0-9a-f]+)',(GEN/'lifted.h').read_text())))
e=ELFFile(io.BytesIO((OUT/'run/libnative.so').read_bytes()));sections=lifter.code_sections(e);text=e.get_section_by_name('.text');textend=text['sh_addr']+text['sh_size'];bodies=[];audit=[];remaining=source
for a in addresses:
    i=bisect.bisect_right(starts,a);end=starts[i] if i<len(starts) else textend
    l=lifter.Lifter();l.image_sections=lifter.data_sections(e);l.plt_names=lifter.plt_symbols(e)
    body=l.lift_function(a,end-a,lifter.bytes_at(sections,a,end-a))
    audit.append({'address':hex(a),'size':end-a,'repaired':body is not None,'unsupported':dict(l.unsupported)})
    if body:
        bodies.append(body);remaining=re.sub(r'void fn0_'+format(a,'x')+r'\(Arm64Ctx\* c\) \{[^\n]*\}\n','',remaining)
(GEN/'padding_entries.c').write_text('#include "lifted.h"\n'+''.join(bodies));stubpath.write_text(remaining)
(OUT/'padding_stub_audit.json').write_text(json.dumps(audit,indent=2));print('Repaired',len(bodies),'of',len(addresses));print([x for x in audit if not x['repaired']])
