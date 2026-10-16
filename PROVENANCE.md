# Dependencies and source notes

`bsm_random.cpp` adapts the default degree-31/separation-3 BSD generator from Android 9 bionic's upstream NetBSD `random.c`, under BSD-3-Clause. Copyright and terms are retained in `vendor/bionic_random/LICENSE` and copied into local game packages. Reference: https://android.googlesource.com/platform/bionic/+/android-9.0.0_r1/libc/upstream-netbsd/common/lib/libc/stdlib/random.c . Android's published stdlib vectors independently verify its output. This replaces the incompatible 15-bit Windows CRT stream for imported rand/random/srand/srandom.

I used androidrecomp by sp00nznet as the translation/runtime foundation, under the MIT license, pinned to tree `4a2004953e396fca04e940c300cad7c9f5fa77f3`. Its source is in `vendor/androidrecomp`, with its original license. The shared adapter changes and current limits are described in `docs/compatibility.md`.

The Android interface work was informed by ChanseyIsTheBest/smk2_nx, tree `52978c3a0c28256e7e00ef1785c49b670726cfb0`, along with independent inspection of the matching Android 1.10 engine. No publisher source or assets are included here. Translated game code, engine binaries, assets, build products and profiles stay in the local `LocalTestBuild` output.

`vendor/minimp3` contains lieff/minimp3's decoder headers, copied from that pinned reference tree. Their headers carry the CC0/public-domain dedication and original upstream URL. SDL2 2.32.10 is a local build dependency under LocalTestBuild/deps; it is not game code.

Local changes to androidrecomp cover conservative computed jumps, literal-pool traps, decoded lane-size recovery, fixed-point conversions, JNI reference identity and capacity, Windows/Android numeric return conventions, EGL/thread ownership, and the BSM2 vertex attribute layout. The host and sound bridges are prototype code; passing a coverage count does not establish complete ARM64 or Android compatibility.

The desktop cursor adapter is original host code. Its three patch sites were independently recovered from the pinned Android 1.10 engine: two gameplay control-presence reads and the movement-target method. No publisher source/decompile is included here. A SHA-256 guard prevents applying those offsets to another engine release. Native movement and gameplay gates remain; the host supplies cursor coordinates without mobile finger offsets and leaves UI touch flags unchanged.
