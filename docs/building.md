# Building on Windows

This takes more setup than running the finished game folder. I am keeping the original game files and generated translation local, so you will need to supply your own matching APK or APKM and build the executable on your machine.

## What you need

- Windows x64 and Visual Studio 2022 or its Build Tools, with Desktop development with C++ and a Windows SDK installed.
- Python 3.11 or newer, with the packages in `requirements.txt`.
- CMake 3.20 or newer, available on your command path.
- The [SDL2 2.32.10 Visual C++ development package](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10). Extract it somewhere convenient and use its `cmake` folder for `--sdl2-dir` below.
- Your complete Android 1.10 APKM, containing the base assets and arm64 split. A standalone APK with both also works.

The supported `libnative.so` SHA-256 is `1b3e1e45589c10945a5941949552136647c40842cd96d437fd96dfce2a4387b9`. A different release needs its own analysis and adapter checks. Renaming another version will not make it compatible.

## Import, translate and build

Open PowerShell in the repository folder. The paths below are examples; use the locations of your own APKM and extracted SDL2 SDK.

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe tools/project.py import "D:\Games\BSM2-1.10.apkm"
.\.venv\Scripts\python.exe tools/project.py generate
.\.venv\Scripts\python.exe tools/project.py configure --sdl2-dir "D:\SDKs\SDL2-2.32.10\cmake"
.\.venv\Scripts\python.exe tools/project.py build
.\.venv\Scripts\python.exe tools/project.py test --sdl2-dir "D:\SDKs\SDL2-2.32.10\cmake"
.\.venv\Scripts\python.exe tools/project.py package --sdl2-dir "D:\SDKs\SDL2-2.32.10\cmake"
```

The default output folder is `LocalTestBuild`, inside the repository. The finished local game folder is `LocalTestBuild/Game`. Double-click its `Play.cmd` to run it. You can add `--work "D:\BSM2-build"` to each command if you want the generated files elsewhere.

The translation is large, and the first compilation can take a while. Keep a few GB free for generated C and build products. Later changes to the host adapters usually rebuild much faster. Generation stops if a generated folder already exists, so use a new work folder when testing a fresh generation.

The tests use their own small fixtures and do not load your save. They check specific interfaces rather than playing through the campaign. The local package command preserves an existing `Game/SaveData` folder.

## Sharing the project

Share the source repository or its source ZIP. Keep `LocalTestBuild` and the generated game folder private: they contain the original commercial files and translated game code. The repository uses an allowlist in `.gitignore` so those files, saves and private working notes are not tracked by default.

The generated game folder is for running your own local build. The repository does not download game files for you.
