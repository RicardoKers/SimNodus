# SN-021 native project declaration acceptance

Date: 2026-09-15. Base main: `c7ac6f4937b6277d9c12542b231eb0c48ccf7011`.
Status: native project 0.1 declaration composition; SN-021 remains in progress.

[ADR 0061](../decisions/0061-native-project-declarations.md) and the
[contract](../architecture/NATIVE_PROJECT_VALIDATION.md) define the bounded slice.
The [audit](evidence/SN-021-project-summary.json) records commands, environment,
test output, hashes and limitations. The unchanged Python reference is the oracle.

## Acceptance

All 20 focused cases passed, with 106 native/reference comparisons. These include
the original 15-case suite, exact combined budget, uint64/grid/token boundaries,
closed nested fields, independent catalog scopes, unused declarations, Unicode
name/path bounds and fail-closed temporal policies. Exact tolerances are preserved.
Native ownership checks passed after caller mutation/result release. Every nested
parameter occurrence retains values, units, origins and absolute offsets relative
to standalone resource-link validation; lock offsets and strict version gates pass.

The MSVC 19.51/SDK 10.0.26100.0 Debug build passed. All 32 Windows CTests passed:
new project checks, 57 resource-link comparisons, 135 lock comparisons/137 digest
cases, 157 binding comparisons, 315 parameter comparisons, 316 topology comparisons,
95 syntax assertions/209 differential cases, 94 original schema tests and foundation
contracts. Native physical tests retain 30 passes/two symlink privilege skips;
Python resources retain 28 passes/four explicit skips. Hosted checks must separately
report their actual Windows/Linux results before protected-main integration.

All 103 selected historical hashes matched; three changed source hashes were
verified in the base. DESKTOP_UX was checked in the base because of preexisting
local work. All ten audited source hashes match the workspace. Twelve preexisting
SN-045 changes remain local and excluded, including shared planning edits.
The repository checker passed for 548 workspace and 546 publication-tree text
files. All ten audited hashes match both trees; staged whitespace and shared
planning review passed. The publication excludes the two local-only documents.

## Preserved failed attempts

The initial sandbox attempt could not write the probe or identify/access the C++
compiler. Host configuration then found a duplicate project/test target block in
CMake. Removing that duplicate allowed configuration and the full build to pass.
Neither failed configuration produced a test result. The subsequent full CTest
run passed without a code/test failure. These initial outcomes remain in the audit;
earlier experiment evidence is not rewritten.

## Limits and next step

This completes native composition of the declarative baseline, not native source
graph loading, saving or compilation. No actual source/ELF/boot interfaces, firmware
or runtime capabilities are verified. Hashes and declared architecture equality
do not prove trusted origin, redistribution permission or execution authorization.
The API opens no resource, renders nothing, loads no DLL/model/firmware, downloads
nothing and starts no engine. Existing physical handle/reparse/alias/race protections
remain independent and unchanged. No real-engine integration claim follows from
synthetic declaration tests; supported profiles and tolerances remain unchanged.

Next implement bounded native source graph loading with stable identities and
source mappings, then safe persistence and compilation with required integration
evidence. Preserve SN-044, local SN-045, shared instrumentation, MCU/toolchain
independence, PDF/PID behavior and SN-017 Python/GDB ownership. Prepare the requested
next-cycle chat prompt only when full SN-021 acceptance is integrated.
Native Unicode remains stricter than historical Python; allocation failures are
structured but not fault-injected, and hard OS memory/wall quotas are not claimed.

## Reproduction

Source `6d9f380` was pushed on `codex/sn-021-native-project`.
[PR #28](https://github.com/RicardoKers/SimNodus/pull/28) records final required
Foundation checks and the authorized protected-main squash. Expected jobs contain
32 Windows and 27 Linux CTests; consult final logs for actual hosted acceptance.

```powershell
cmake -S . -B build/sn021-project-validated
cmake --build build/sn021-project-validated --config Debug
ctest --test-dir build/sn021-project-validated -C Debug --output-on-failure --verbose
python tools/check_repository.py
git diff --check
```
