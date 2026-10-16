"""Paths for local generated outputs; never put game data in tracked source."""
import os,pathlib,sys
SOURCE=pathlib.Path(__file__).resolve().parent
ROOT=SOURCE.parents[2] if SOURCE.parts[-3:]==('main','bsm25','mobile_recomp') else SOURCE
legacy=ROOT/'Output/MobileRecomp/P001/LocalTestBuild'
OUT=pathlib.Path(os.environ.get('BSM2_BUILD_ROOT',legacy if legacy.exists() else SOURCE/'LocalTestBuild')).resolve()
# Optional analysis dependencies in the original local workspace. Normal clones
# install requirements.txt into their own Python environment instead.
local_deps=ROOT/'BSM2Mobile/LocalTestBuild/Investigation_2026_10_15/python_deps'
if local_deps.exists():sys.path.insert(0,str(local_deps))
