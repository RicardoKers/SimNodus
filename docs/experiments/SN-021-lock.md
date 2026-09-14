# SN-021 native resource lock acceptance

Date: 2026-09-14. Base main: `bd1da0df6ad8a18b9884ca441660207cdf255fbe`.
Status: bounded resource lock 0.1 implementation; SN-021 remains in progress.

[ADR 0059](../decisions/0059-native-resource-lock.md) and the
[contract](../architecture/NATIVE_RESOURCE_LOCK.md) define exact metadata,
inventory fingerprints and immutable declared requests with source offsets.
The operation accepts bytes only and never invokes physical verification.
All containment/resource/redistribution/simulation flags remain false.

## Focused acceptance

The resumed full Debug build passed with MSVC 19.51.36246.0 and Windows SDK
10.0.26100.0. The initial configure succeeded, but its build-output session became
unavailable after an automatic approval usage-limit interruption; its final status
was not inferred. The resumed build verified every target before tests ran.

All 23 focused lock tests passed: the unchanged 18-case Python suite plus five
adversarial cases, 135 native/reference metadata comparisons and 137 digest cases.
Known empty/abc digest vectors, immutable ownership, source offsets, integer -0
and an inert missing DLL declaration passed in C++. No resource was opened by
the API. Trusted fixture byte checks are explicit test actions, not parser I/O.

One documented diagnostic difference is preserved: the Python host rejects a
5000-digit root integer before schema validation (`input`); native syntax rejects
the root shape (`shape`). Both reject. All other compared rejection categories,
accepted requests and statistics agree. This does not relax integer byte limits.

Digest cases independently compare every padding position, binary/multiblock data
and 1 MiB against hashlib, plus overflow rejection. Additional metadata cases cover
exact dependency/file/byte limits, fractional/exponent/bool/overflow rejection,
case-sensitive canonical ordering, metadata/ID independence, path/text boundaries
and nested unknown fields. No failed build/test result was observed in the resumed
checks; the interrupted original build remains unconfirmed as described above.

## Full acceptance and remaining gates

The [audit](evidence/SN-021-lock-summary.json) records full local regressions,
source/binary hashes and preserved historical inputs. Hosted checks and protected
main integration are recorded separately in the publication record below.
Twelve preexisting SN-045 documentation changes remain local and excluded,
including shared CURRENT/BACKLOG edits. No historical evidence is overwritten.

All 28 local Windows CTest entries passed, including 135 lock comparisons,
137 digest cases, 157 binding comparisons, 315 parameter comparisons, 316 topology
comparisons, 95 syntax assertions, 209 syntax differential cases and 94 original
schema tests. Native physical-resource tests retained 30 passes/two unavailable
symlink privileges; Python resource tests retained 28 passes/four explicit skips.
All 86 selected historical hashes matched. DESKTOP_UX was verified in the baseline
because of the local SN-045 overlay; the earlier CMake hash was separately checked
in that baseline. All ten new source hashes and twelve preexisting files matched.
Workspace repository checker: 529 text files passed. `git diff --check` passed.
The exact staged publication tree passed its checker for 527 text files, excluding
the two local-only SN-045 documents. All ten audited source hashes match both
workspace and publication tree. Staged whitespace and shared-planning review passed.

This is metadata validity, not physical containment, measured resource bytes,
actual source interfaces, origin/redistribution approval or execution permission.
No dependencies are downloaded, libraries/models/firmware loaded or content
rendered. The existing explicit physical snapshot API remains unchanged. No
engine campaign is required for this inert semantic slice; no integration or
additional supported engine/device profile is claimed.

Next port typed resource links, then project semantics before native graph loading.
Actual source/ELF/boot verification, atomic saving and compilation remain pending.
Preserve SN-044, SN-045 local work, shared instrumentation, MCU/toolchain independence,
numerical restrictions, PDF/PID fixes and SN-017 Python/GDB fixture ownership.
Allocation failure is structured but not fault-injected; hard OS memory/wall quotas
are not claimed. Native lone-surrogate rejection remains stricter than Python.

## Reproduction

```powershell
cmake -S . -B build/sn021-lock
cmake --build build/sn021-lock --config Debug
ctest --test-dir build/sn021-lock -C Debug --output-on-failure --verbose
python tools/check_repository.py
git diff --check
```
