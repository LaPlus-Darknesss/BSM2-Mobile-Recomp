"""Recover omitted standard ARM64 stack-frame entries in this exact image.

Only conventional entry prologues are admitted. Each extent ends at the next
conventional entry, and existing generated entry points are left unchanged.
The manifest and disassembly stay in LocalTestBuild for independent review.
"""
import pathlib,sys,re,io,json,bisect
from project_paths import ROOT, OUT, SOURCE
GEN=OUT/'generated'

from elftools.elf.elffile import ELFFile
from capstone import Cs,CS_ARCH_ARM64,CS_MODE_LITTLE_ENDIAN
elf=ELFFile(io.BytesIO((OUT/'run/libnative.so').read_bytes()));text=elf.get_section_by_name('.text');start=text['sh_addr'];end=start+text['sh_size'];code=text.data();md=Cs(CS_ARCH_ARM64,CS_MODE_LITTLE_ENDIAN);md.skipdata=True
known=set(int(a,16) for a in re.findall(r'void fn0_([0-9a-f]+)\(', (GEN/'lifted.h').read_text()))
reliable=[]
for entry in elf.get_dwarf_info().EH_CFI_entries():
    h=getattr(entry,'header',{})
    if 'initial_location' in h:reliable.append((h['initial_location'],h['initial_location']+h['address_range']))
for symbol in elf.get_section_by_name('.dynsym').iter_symbols():
    if symbol['st_info']['type']=='STT_FUNC' and symbol['st_size']:reliable.append((symbol['st_value'],symbol['st_value']+symbol['st_size']))
merged=[]
for a,b in sorted(reliable):
    if merged and a<=merged[-1][1]:merged[-1]=(merged[-1][0],max(b,merged[-1][1]))
    else:merged.append((a,b))
starts=[a for a,b in merged]
def covered(a):
    i=bisect.bisect_right(starts,a)-1
    return i>=0 and a<merged[i][1]
prologues=[]
for a,n,m,op in md.disasm_lite(code,start):
    if (m=='stp' and op.startswith('x29, x30, [sp, #-') and op.endswith('!')) or (m=='sub' and op.startswith('sp, sp, #')):prologues.append(a)
entries={}
for i,a in enumerate(prologues):
    if a in known or covered(a):continue
    stop=prologues[i+1] if i+1<len(prologues) else end
    j=bisect.bisect_right(starts,a)
    if j<len(starts):stop=min(stop,starts[j])
    # The recovered range must decode to ordinary instructions with a return.
    ins=list(md.disasm_lite(code[a-start:stop-start],a))
    returns=[p for p,_,m,_ in ins if m=='ret']
    if not returns:continue
    # Compilers place floating constants after the final return. Keep these
    # literal bytes in the extent, but reject undecodable bytes inside code.
    if any(m=='.byte' and p<max(returns) for p,_,m,_ in ins):continue
    entries[format(a,'x')]=stop-a
(OUT/'recovered_callbacks.json').write_text(json.dumps(entries,indent=2))
print('Conventional prologues',len(prologues),'omitted candidates',len(entries));print(list(entries.items())[:20])
if '--apply' in sys.argv:
    p=OUT/'extra_entries.json';existing=json.loads(p.read_text()) if p.exists() else {};existing.update(entries);p.write_text(json.dumps(existing,indent=2))
