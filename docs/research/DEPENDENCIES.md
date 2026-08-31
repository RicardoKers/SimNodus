# Dependency inventory

Updated: 2026-08-31, SN-010. This is a **Windows x64 experiment baseline**, not a supported simulator distribution. See the [execution report](../experiments/SN-010-results.md), [setup recipe](../development/WINDOWS_BACKENDS.md), and [ADR 0006](../decisions/0006-windows-backend-baseline.md).

## Selected and observed versions

| Dependency | Exact baseline | Evidence and limits |
|---|---|---|
| Windows | Windows 11 Home Single Language, 10.0.26200, x64 | Development host only; classroom minimum OS not established |
| C++ compiler | MSVC 19.51.36246.0; toolset directory 14.51.36231; Visual Studio 18 2026 Build Tools | C++20 ngspice probe compiled and ran with `/W4 /WX /permissive-`, Debug/x64 |
| Windows SDK | 10.0.26100.0 | Selected by CMake |
| CMake | 4.2.3-msvc3 | Project minimum remains 3.24; explicit `Visual Studio 18 2026`, x64 generator tested |
| Python | 3.14.4, x64 | Repository and artifact checkers; standard library only; minimum 3.10 |
| Git | 2.53.0.windows.1 | Local development tool |
| 7-Zip | 26.02, x64 | Extracted the official ZIP and 7z packages |
| Renode | 1.16.1.19220, Release, build `d66b0c2a-202602161036` | Official Windows portable .NET package; version and headless start/quit passed |
| Renode runtime | Bundled .NET 8.0.10 | Self-contained package uses this runtime, not the host's installed 8.0.22; security/update review needed before distribution |
| Renode client/platform | Commit `d66b0c2aa3d420408eccecfd1d3bab0fd702a6db` | Sources included in the same package; native MSVC client build fails; SN-019 tracks adaptation |
| Renode infrastructure | Submodule `add012af003a0f620d3da52828262676f374d121` | Revision from the parent release tree; source audit only |
| ngspice | 47, official Windows VS x64 shared library, created Aug 11 2026 14:25:06 | Real C++ load/init/version/quit passed; no circuit run |
| ngspice audio dependencies | libsndfile 1.2.2; libsamplerate 0.2.2 | Version functions queried; obtain DLLs from the matching official console package |
| OpenMP runtime | `libomp140.x86_64.dll`, pinned by file hash | Included in the ngspice DLL package; semantic runtime version not independently established |
| STM32CubeIDE | Installed package 2.1.1 | Installation discovered; GUI/debugger session not tested |
| ARM GCC | GNU Tools for STM32 14.3.rel1.20251027-0700; GCC 14.3.1 20250623 | Bundled compiler `--version` ran; ELF compilation/boot remains E-02 |
| ARM GDB | Same toolchain; 15.2.90.20241229-git | `--version` ran; no target/debugger connection |
| Qt | Installed directory 6.11.1 observed | Not selected, built, or used; modules/compiler/license review deferred to SN-022 |
| WASM / Verilator | Deferred | No runtime selected or downloaded |

## Origins and integrity

[backend-baseline.json](../../tools/backend-baseline.json) records four exact archive URLs and SHA-256 hashes, plus ten important extracted files. All 14 matched locally. Archive hashes match the GitHub asset digest or SourceForge download page; extracted hashes were computed locally from those archives. This is content integrity evidence, not a malware scan or a full dependency/security attestation.

- [Renode v1.16.1 release](https://github.com/renode/renode/releases/tag/v1.16.1): executable, client, platforms, and bundled notices.
- [Pinned client header](https://github.com/renode/renode/blob/d66b0c2aa3d420408eccecfd1d3bab0fd702a6db/tools/external_control_client/include/renode_api.h): microsecond base units and `timestamp_us`. Do not substitute the changing `latest` API documentation.
- [Pinned STM32F103 platform](https://github.com/renode/renode/blob/d66b0c2aa3d420408eccecfd1d3bab0fd702a6db/platforms/cpus/stm32f103.repl): generic starting point, not a verified Blue Pill profile.
- [ngspice 47 release directory](https://sourceforge.net/projects/ngspice/files/ng-spice-rework/47/): DLL archive, console archive, and source tarball. The source/header and binary/header hashes match exactly.
- SourceForge publishes hashes on the [DLL](https://sourceforge.net/projects/ngspice/files/ng-spice-rework/47/ngspice-47_dll_64.7z/download), [console](https://sourceforge.net/projects/ngspice/files/ng-spice-rework/47/ngspice-47_64.7z/download), and [source](https://sourceforge.net/projects/ngspice/files/ng-spice-rework/47/ngspice-47.tar.gz/download) download pages.

No third-party source, header, DLL, model, firmware, or package is committed. Local copies remain under ignored `build/deps/`. CMake does not download them.

## Build and runtime boundaries

ngspice was **not rebuilt**. The actual `version -f` output reports KLU, CIDER, XSPICE, OpenMP, predictor support, no X11, and shared-library mode. The source's `visualc/sharedspice.vcxproj` uses the v143 toolset and static C runtime configurations, with audio-library links. That source inspection does not recover the producer's exact compiler patch or every build flag; bit-for-bit source reproduction remains unverified.

The DLL imports `sndfile.dll`, `samplerate.dll`, the bundled OpenMP DLL, and Windows system libraries. The first two are missing from the DLL archive and are supplied by `ngspice-47_64.7z`. The probe uses a process-local DLL search directory; no PATH or global installation changes are required.

The probe does not load XSPICE `.cm`, OSDI, VPI, or optional models. The upstream `spinit` contains fixed `C:/Spice64` paths and model loads; do not use it for this experiment. A one-line owned `spinit` disables user initialization. A call to the documented pre-initialization function `ngSpice_nospinit()` crashed this binary; see the report before changing the startup sequence.

## License and redistribution review

Original SimNodus material remains MIT. Development use and selecting an experiment dependency do not approve a future binary bundle. Preserve upstream notices and complete the [distribution checklist](../development/LICENSING.md) before packaging.

| Material | Observed terms / required follow-up |
|---|---|
| Renode original code/client | MIT, Antmicro; root license and source notice inspected |
| Renode bundled dependencies | Separate `licenses/` notices, including LGPL 2.1 tlib and other components; retain/audit the full bundle and corresponding-source obligations before redistribution |
| ngspice source and binary | Source `COPYING` describes modified BSD plus exceptions: KLU/numparam LGPL, OSDI MPL 2.0, optional XSPICE table GPL v2 or later; do not label the complete archive simply MIT/BSD |
| Optional `.cm`/OSDI/VPI assets | Not loaded or approved for redistribution; per-file provenance and terms still required |
| libsndfile | [Upstream LGPL terms](https://libsndfile.github.io/libsndfile/); source and bundled codec obligations require a distribution audit |
| libsamplerate | [Upstream BSD-2-Clause terms](https://libsndfile.github.io/libsamplerate/license.html); retain applicable notices |
| OpenMP/native runtimes | Preserve exact runtime provenance/notices; redistributable packaging not approved by this experiment |
| ngspice manual / COPYING document | Source declares CC-BY-SA 4.0 for these documents; not copied into SimNodus |
| CubeIDE / GNU Tools for STM32 | Existing local installation only; GDB reports GPLv3+; toolchain/runtime and ST package terms must be reviewed before any redistribution. No ST firmware copied |
| Visual Studio / SDK / 7-Zip | External development tools, not shipped as SimNodus artifacts |

These are engineering inventory findings, not a guarantee of legal or security compliance. Do not distribute the entire downloaded directories as a shortcut.
