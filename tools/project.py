"""Import your own Android files, generate locally, build and package on Windows."""
import argparse,hashlib,io,json,os,pathlib,shutil,subprocess,sys,zipfile
SOURCE=pathlib.Path(__file__).resolve().parents[1]
EXPECTED='1b3e1e45589c10945a5941949552136647c40842cd96d437fd96dfce2a4387b9'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('command',choices=['import','generate','configure','build','test','package'])
parser.add_argument('bundle',nargs='?',type=pathlib.Path)
parser.add_argument('--work',type=pathlib.Path,default=SOURCE/'LocalTestBuild')
parser.add_argument('--sdl2-dir',type=pathlib.Path,default=os.environ.get('SDL2_DIR'))
parser.add_argument('--cmake',default=os.environ.get('CMAKE','cmake'))
args=parser.parse_args();work=args.work.resolve();work.mkdir(parents=True,exist_ok=True)
env=dict(os.environ);env['BSM2_BUILD_ROOT']=str(work)
def run(command):subprocess.run(list(map(str,command)),cwd=SOURCE,env=env,check=True)
def helper(name,*rest):run([sys.executable,SOURCE/name,*rest])
def check_engine():
 p=work/'run/libnative.so'
 if not p.exists() or hashlib.sha256(p.read_bytes()).hexdigest()!=EXPECTED:raise SystemExit('This build requires the pinned arm64 1.10 libnative.so. Import a matching APK/APKM first.')
 return p
def safe_entry(folder,name):
 p=(folder/pathlib.PurePosixPath(name)).resolve()
 if not p.is_relative_to(folder.resolve()):raise ValueError('Unsafe archive path: '+name)
 return p
if args.command=='import':
 if args.bundle is None:parser.error('import requires your APK or APKM path')
 engine=None;assets={}
 def inspect(z):
  global engine
  for item in z.infolist():
   if item.is_dir():continue
   if item.file_size>512*1024*1024:raise ValueError('Archive entry exceeds import limit')
   if item.filename=='lib/arm64-v8a/libnative.so':engine=z.read(item)
   elif item.filename.startswith('assets/'):assets[item.filename[7:]]=z.read(item)
 with zipfile.ZipFile(args.bundle) as z:
  if args.bundle.suffix.lower()=='.apkm':
   for name in z.namelist():
    if pathlib.PurePosixPath(name).name in ('base.apk','split_config.arm64_v8a.apk'):
     with zipfile.ZipFile(io.BytesIO(z.read(name))) as inner:inspect(inner)
  else:inspect(z)
 if engine is None or hashlib.sha256(engine).hexdigest()!=EXPECTED:raise SystemExit('No matching Android 1.10 arm64 engine found. No files imported.')
 if 'Assets/data.jet' not in assets:raise SystemExit('Base APK assets are missing. Supply the full APKM bundle.')
 for name in assets:safe_entry(work/'run/assets',name)
 (work/'run').mkdir(exist_ok=True);(work/'run/libnative.so').write_bytes(engine)
 for name,data in assets.items():p=safe_entry(work/'run/assets',name);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 print('Imported matching engine and',len(assets),'assets into',work)
elif args.command=='generate':
 engine=check_engine()
 if (work/'generated').exists():raise SystemExit('Generated output already exists. Use a new --work folder to preserve it.')
 run([sys.executable,SOURCE/'vendor/androidrecomp/tools/lifter.py',engine,'--out',work/'generated','--shards','128'])
 shutil.copy2(SOURCE/'tools/engine_110_entries.json',work/'extra_entries.json')
 helper('add_entries.py');helper('repair_padding_stubs.py');helper('install_cursor_hook.py');helper('limit_optimizer.py')
elif args.command=='configure':
 check_engine()
 if not (work/'generated/dispatch.c').exists():raise SystemExit('Generate the local translation first.')
 command=[args.cmake,'-S',SOURCE,'-B',work/'build','-G','Visual Studio 17 2022','-A','x64','-DARC_LIFTED_DIR='+str(work/'generated')]
 if args.sdl2_dir:command+=['-DSDL2_DIR='+str(args.sdl2_dir.resolve())]
 run(command)
elif args.command=='build':run([args.cmake,'--build',work/'build','--config','Release','--target','arc_boot','--parallel','6'])
elif args.command=='test':
 command=[args.cmake,'-S',SOURCE/'tests','-B',work/'tests','-G','Visual Studio 17 2022','-A','x64']
 if args.sdl2_dir:command+=['-DSDL2_DIR='+str(args.sdl2_dir.resolve())]
 run(command);run([args.cmake,'--build',work/'tests','--config','Release'])
 if args.sdl2_dir:
  dll=args.sdl2_dir.resolve().parent/'lib/x64/SDL2.dll'
  if dll.exists():shutil.copy2(dll,work/'tests/Release/SDL2.dll')
 for name in ('audio','math','cursor','random','libc','window'):run([work/('tests/Release/'+name+'_fixture.exe')])
elif args.command=='package':
 check_engine();dest=work/'Game';dest.mkdir(exist_ok=True)
 shutil.copy2(work/'build/vendor/androidrecomp/Release/bsm2_mobile.exe',dest/'BSM2Mobile.exe')
 if not args.sdl2_dir:parser.error('package requires --sdl2-dir pointing at the SDL2 SDK cmake folder')
 shutil.copy2(args.sdl2_dir.resolve().parent/'lib/x64/SDL2.dll',dest/'SDL2.dll')
 shutil.copy2(work/'run/libnative.so',dest/'libnative.so');shutil.copytree(work/'run/assets',dest/'Assets',dirs_exist_ok=True)
 shutil.copy2(SOURCE/'launch_local.ps1',dest/'Run.ps1');shutil.copy2(SOURCE/'docs/saves.md',dest/'SAVES.txt')
 (dest/'Play.cmd').write_text('@echo off\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run.ps1"\n')
 (dest/'Open saves.cmd').write_text('@echo off\nexplorer.exe "%~dp0SaveData"\n')
 for name in ('SaveData','Logs','Licenses'):(dest/name).mkdir(exist_ok=True)
 for path,label in [(SOURCE/'LICENSE','project.txt'),(SOURCE/'vendor/androidrecomp/LICENSE','androidrecomp.txt'),(SOURCE/'vendor/bionic_random/LICENSE','bionic_random.txt'),(SOURCE/'PROVENANCE.md','PROVENANCE.md'),(args.sdl2_dir.resolve().parent/'LICENSE.txt','SDL2.txt')]:shutil.copy2(path,dest/'Licenses'/label)
 header=(SOURCE/'vendor/minimp3/minimp3.h').read_text();(dest/'Licenses/minimp3.txt').write_text(header.split('/*',1)[1].split('*/',1)[0].strip()+'\n')
 print('Local game folder:',dest,'\nKeep this generated package private; it contains your original game data and translated game code.')
