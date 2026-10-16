# Compatibility and current limits

I checked the 380 imports in the supported Android 1.10 engine against the Windows adapters, looking at argument and return types, structure sizes, text handling, files, timing, graphics, threads and the Android interfaces. An import resolving successfully is not enough on its own, since a Windows function can have the same name and still expect different data.

These are the shared differences that have been repaired so far:

| Area | Difference and repair |
| --- | --- |
| Random values | Windows `rand()` only supplies 15-bit values. The engine expects Android's 31-bit stream, which affects projectile spread and other randomized behavior. The adapter uses Android's shared BSD stream and passes published reference vectors. |
| Integer parsing | Android arm64 `long` is 64-bit; Windows `long` is 32-bit. `strtol` and `strtoul` now parse the full Android range, including overflow and end pointers. |
| Formatting | Integer formats such as `%ld` and `%lu` retain their 64-bit values. Integer scanning and `%ln` writes also use the guest width. |
| Wide text | Android uses 32-bit wide characters and Windows uses 16-bit wide characters. Wide memory operations, numeric parsing, UTF-8 conversion, wide output and `%ls` output now work on guest-sized characters without passing those buffers to Windows wide-string functions. |
| Process clock | Android `clock()` reports CPU time in microseconds. Windows uses different units and measures elapsed time. The adapter reads process CPU time and returns Android units. |
| Calendar values | Calendar input/output uses the Android structure layout, including its timezone fields. `mktime` normalizes back into that structure, and formatting receives a translated host structure. |
| File positions | `fseek` and `ftell` preserve 64-bit offsets. A fixture checks a position above 4 GB. |
| Replacing files | Android `rename` can replace an existing destination. The Windows adapter now uses replacement semantics, which matters for save rotation. |
| Floating-point calls | Math imports use the ARM64 floating-point registers. Android binary128 parser results are encoded for the guest rather than returned as a Windows `long double`. |
| Files and structures | Binary file mode, Linux open flags, directory entries and arm64 file metadata use explicit adapters. These were already present and were reviewed again. |
| Graphics and input | The game keeps its original vertex attribute indices and render-thread context handling. The window is fixed at 540 by 960 logical window pixels, with the desktop cursor adapter mapping into the playfield. |

The compatibility fixtures cover published RNG sequences, distribution and radial spread, host floating-point calls, text and numeric ABI cases, large file offsets, file replacement, the fixed-window settings, cursor mapping and independent audio callbacks. Fresh generation from the supplied matching APKM produced the same 25,861 function bodies as the working local translation, with no missing or duplicate definitions. The clean CMake configuration also passed. That comparison is not a second full compilation of all generated files.

## What still needs testing

This is a release candidate, so I am keeping the remaining limits visible:

- The full campaign, later bosses, every equipment combination and situational power behavior still need playthrough testing.
- JNI and the Android interfaces are a bounded implementation for this game, not a complete Android environment. Accounts, purchases, ads and cloud services are stand-ins.
- Locale handling is limited. The UTF-8 and UTF-32 checks do not establish every language, locale rule or text layout.
- Rare scan formats involving wide strings or binary128 floating-point destinations, and full binary128 precision, remain limited. Ordinary asset/gameplay parsing has been smoke checked, but that does not cover every C library format.
- Process creation, signals and networking are partial. The local game should not be treated as having working Android services behind those interfaces.
- Thread adapters preserve the interfaces used in the checked runs, however, not every POSIX scheduling, cleanup or synchronization feature is implemented.
- Window resizing is disabled. Fullscreen, stretching and moving between displays with different scaling settings have not been accepted.
- Local save writes and reloads have been checked. Exact mid-level resume and recovery from an interrupted write need more testing.

The instruction and interface fixtures are useful for catching specific mistakes, however, passing them does not prove complete ARM64 or Android compatibility.

## References

The Android behavior is checked against [bionic](https://android.googlesource.com/platform/bionic/), including its [integer parsers](https://android.googlesource.com/platform/bionic/+/master/libc/bionic/strtol.cpp), [clock implementation](https://android.googlesource.com/platform/bionic/+/d3026c61f9f219e729c1675086f7034e4264d18e/libc/bionic/clock.cpp) and [time declarations](https://android.googlesource.com/platform/bionic/+/5a29d541fc6a54c922bce28c74cd22e763196529/libc/include/time.h). Microsoft's [clock documentation](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/clock?view=msvc-170) and [type sizes](https://learn.microsoft.com/en-us/cpp/cpp/data-type-ranges?view=msvc-170) describe the host differences.
