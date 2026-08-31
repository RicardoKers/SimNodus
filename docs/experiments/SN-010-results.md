# SN-010: Windows dependency baseline results

Date: 2026-08-31. Task: [SN-010 / #1](https://github.com/RicardoKers/SimNodus/issues/1). **Outcome: inventory and version selection complete, with explicit integration limitations. E-01 through E-06 remain not run.**

## Scope and reproduction

Select exact official backend packages, audit the host and APIs, verify hashes, and determine whether basic native startup is possible. No firmware, netlist, circuit, GPIO, ADC, or debugger session was used. Host/toolchain details are in [DEPENDENCIES](../research/DEPENDENCIES.md); commands and expected outcomes are in [WINDOWS_BACKENDS](../development/WINDOWS_BACKENDS.md). SHA-256 values are in the [manifest](../../tools/backend-baseline.json).

The probe is owned C++20 source, built Debug/x64 with MSVC 19.51.36246.0, `/W4 /WX /permissive-`, Windows SDK 10.0.26100.0, and CMake 4.2.3-msvc3. Backend binaries are upstream Release packages, not locally rebuilt.

## Observed results

| Check | Result | Meaning |
|---|---|---|
| Four official archive hashes | Pass, match published SHA-256 values | Downloaded bytes identified; no signature/security audit implied |
| Ten selected extracted files | Pass | Includes binary/header/client/platform/runtime identifiers |
| ngspice source versus binary header | Identical SHA-256 | Both use the release-47 header |
| Renode `--version` | Pass, exit 0 | 1.16.1.19220, expected build, bundled .NET 8.0.10 |
| Renode headless start/quit | Pass, exit 0 | Process starts without GUI; no machine loaded |
| Official Renode C client with MSVC | Fail, D8021 for `/Werror` | Native adaptation is required; no working client claimed |
| C++ ngspice probe build | Pass | Real upstream header compiles in this native C++20 host |
| ngspice DLL load/init/version/quit | Pass with documented setup | Expected API exports exist and startup/idle shutdown works |
| Missing ngspice file | Rejected, exit 1 | Clear failure without starting the engine |
| Missing audio dependencies | Rejected, exit 1 | A DLL package alone is not sufficient |
| Unapproved `spinit` content | Rejected, exit 1 | Probe only accepts its owned initialization command |
| Working-directory `.spiceinit` with a marker | Ignored; normal probe passes | Marker not executed; user-init suppression confirmed |
| Artifact checker with missing archives | Rejected, exit 1 | Missing downloads do not produce a passing baseline |
| ARM GCC / GDB version commands | Pass | Executables present and runnable; firmware/debugging untested |

The four probe success/failure cases ran in separate processes with 15-second timeouts. Generated output stays under ignored build directories. A sanitized successful ngspice summary is:

```text
init=0 version=0 quit=1 running=0 exit_callback=1 exit_status=0 quit_requested=1
PASS: ngspice-47 loaded, expected exports found, initialized, reported version, and quit.
No circuit, code model, firmware, or co-simulation was executed.
```

`quit=1` is explained by the selected source's `ngSpice_Command` / `shared_exit` longjmp path. It is not a generic rule for other commands. Export existence for `ngSpice_Init_Sync` and `ngSpice_SetBkpt` establishes no timing, synchronization, or rollback capability.

## Failures found during setup

1. Generic SourceForge requests returned HTML despite success status. Archive inspection rejected them. Direct official mirror downloads with `curl.exe` matched the published hashes; no HTML hash became a dependency pin.
2. The ngspice DLL imports `sndfile.dll` and `samplerate.dll`, absent from its DLL archive. The matching official console archive supplies them. Version queries report libsndfile 1.2.2 and libsamplerate 0.2.2. The probe adds that directory to its process-local DLL search; it does not copy DLLs into system directories.
3. Calling `ngSpice_nospinit()` before `ngSpice_Init`, as described by the header, caused Windows access violation `0xC0000005` (process return `-1073741819`). The DLL had loaded successfully; the failure occurred before initialization. Root cause and other pre-init functions are not established. The successful probe uses the owned `spinit` route instead. E-01 must preserve this finding and audit lifecycle behavior before extracting an adapter.
4. An initial sandboxed Renode version check printed its version but failed during shutdown because its normal per-user configuration directory was not writable. After permitting that ordinary write, both version and headless startup checks exited 0. Version text alone was not accepted as a passing run.
5. With CMake 4's compatibility override, the official Renode client configured but failed under MSVC. Source inspection also found POSIX socket headers and GCC built-ins. [SN-019](https://github.com/RicardoKers/SimNodus/issues/11) must provide a tested Windows transport adaptation before E-02 can claim external control.

## Platform and API audit findings

The exact Renode release header uses `TU_MICROSECONDS = 1`, `TU_MILLISECONDS = 1000`, `TU_SECONDS = 1000000`, and GPIO `timestamp_us`. Newer online documentation describing nanosecond units does not apply to this pin.

The packaged `platforms/cpus/stm32f103.repl` contains generic oversized flash/SRAM maps, multiple GPIO/timer instances beyond a C8 board profile, no ADC instance, an external SVD download in its initialization, and a fixed RCC tag. It was inspected, **not loaded**. E-02 must create a documented offline C8 configuration and explicitly audit clocks, memory, and pin coverage. See the [support matrix](../research/STM32_SUPPORT.md).

## Decision and next work

Keep Renode 1.16.1 and ngspice 47 as the bounded experiment baseline; do not silently follow newer APIs or ship these downloaded bundles. SN-010 meets its inventory/recipe/provenance acceptance criteria. The separate integration gates remain open:

- SN-011 / E-01: RC reference, callbacks, advancement, pause/reset/error handling, and startup limitation investigation.
- SN-019: native Windows Renode client build and real loopback/time smoke check.
- SN-012 / E-02: owned ELF, offline C8 profile, timed GPIO/input, and peripheral audit after the client prerequisite.
- Before binary distribution: exact transitive licensing, runtime security/update review, and clean-machine installation tests.

No numerical error, throughput, real-time performance, or peripheral accuracy was measured. The foundation CI still checks documentation and build scaffolding only; the native probe results above are local Windows evidence.
