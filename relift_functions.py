"""Replace selected generated functions using the current pinned lifter."""
import pathlib,sys,re,io
from project_paths import ROOT, OUT, SOURCE

sys.path.insert(0,str(SOURCE/'vendor/androidrecomp/tools'))
from elftools.elf.elffile import ELFFile
import lifter
with (OUT/'run/libnative.so').open('rb') as f:
    e=ELFFile(f); sections=lifter.code_sections(e);l=lifter.Lifter();l.image_sections=lifter.data_sections(e);l.plt_names=lifter.plt_symbols(e)
    funcs=dict(lifter.dedupe_starts(lifter.functions_from_eh_frame(e),lifter.functions_from_plt(e),lifter.functions_from_symbols(e)))
    if '--branches' in sys.argv:
        count=0
        for p in (OUT/'generated').glob('lifted_*.c'):
            source=p.read_text()
            def update(match):
                global count
                old=match.group(0)
                if not re.search(r'/\* [0-9a-f]+: br ',old):return old
                addr=int(match.group(1),16)
                addresses=re.findall(r'/\* ([0-9a-f]+):',old)
                size=funcs.get(addr,int(addresses[-1],16)+4-addr)
                body=l.lift_function(addr,size,lifter.bytes_at(sections,addr,size))
                if body is None:raise RuntimeError(str(l.unsupported))
                count+=1;return body
            changed=re.sub(r'void fn0_([0-9a-f]+)\(Arm64Ctx\* c\) \{\n.*?\n\}\n',update,source,flags=re.S)
            if changed!=source:p.write_text(changed,encoding='utf8');print('Updated',p.name,flush=True)
        print('Rebuilt computed branches in',count,'functions');raise SystemExit()
    for arg in sys.argv[1:]:
        addr=int(arg,16);size=funcs.get(addr)
        if size is None:
            for p in (OUT/'generated').glob('lifted_*.c'):
                match=re.search(r'void fn0_'+format(addr,'x')+r'\(Arm64Ctx\* c\) \{\n(.*?)\n\}',p.read_text(),re.S)
                if match:
                    addresses=re.findall(r'/\* ([0-9a-f]+):',match.group(1));size=int(addresses[-1],16)+4-addr;break
            if size is None:raise RuntimeError('Unknown recovered range '+arg)
        body=l.lift_function(addr,size,lifter.bytes_at(sections,addr,size))
        if body is None:raise RuntimeError(str(l.unsupported))
        needle='void fn0_'+format(addr,'x')+'(Arm64Ctx* c) {'
        for p in (OUT/'generated').glob('lifted_*.c'):
            source=p.read_text()
            if needle not in source:continue
            start=source.index(needle);end=source.index('\n}\n',start)+3
            p.write_text(source[:start]+body+source[end:],encoding='utf8');print('Updated',hex(addr),p.name);break
        else:raise RuntimeError('Function not found '+arg)
