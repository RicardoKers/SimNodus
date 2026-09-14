# SN-021 native descriptor and binding acceptance

Date: 2026-09-14. Base main: `9e82f3af1cd16a9bce69fa7c4fa9f4a9e3aa09b2`.
Status: bounded topology 0.3 implementation; SN-021 remains in progress.

[ADR 0058](../decisions/0058-native-descriptor-bindings.md) and the
[contract](../architecture/NATIVE_BINDINGS.md) define the complete preserved
descriptor semantics, strict public version boundaries and independent runtime
gates. Structure and parameters reuse original owned syntax without projection.
The immutable result retains explicit nulls/maps/catalogs and original positions,
effective per-occurrence parameters and statistics; simulation readiness is false.

## Focused acceptance and earlier attempts

All 22 binding cases passed, including the unchanged 17-case Python binding suite
and five additional adversarial cases: 157 native/reference comparisons of full
statistics, parameter values/origins and error categories. Native C++ ownership,
map/null positions, missing-reference offset and version-isolation checks passed.
Coverage includes exact combined budget boundaries, interval neighbors, unused
invalid slots, closed units, Unicode names, independent namespaces, empty/null
interfaces, symbol replacement and unexpected fields at every fixture object.

An initial helper command failed PowerShell parsing before writing the probe.
The restricted token then could not write the probe or detect MSVC; the configure
in `build/sn021-bindings` failed and its attempted build had no project. Ordinary
host access generated the probe and configured/built successfully in a separate
`build/sn021-bindings-host` directory. No dependency installation or settings change.

The first focused test run failed two newly constructed tests: an empty symbol
map was attached to a nonempty circuit interface, and a changed component range
still had incompatible existing forwarded overrides. Both native and Python
rejected these inputs; there was no acceptance disagreement. The second attempt
fixed overrides but still selected a nonempty root circuit, leaving one failure.
The final tests use an explicitly empty unused circuit and isolated overrides.
Production code was unchanged by these corrections. Failure logs are retained
and summarized in the new audit; they are not relabelled as passes.

## Full acceptance and preservation

The [audit](evidence/SN-021-bindings-summary.json) records the final full native
regression, source/binary hashes and historical input verification. Consult actual
results there and the PR's final hosted checks; expected job contents are not
acceptance evidence. Older reports and evidence remain unchanged.

All 26 local Windows CTest entries passed: 157 binding comparisons, 315 parameter
comparisons, 316 topology comparisons, 95 syntax assertions, 209 syntax differential
cases and 94 original schema tests. Physical-resource tests retained their explicit
local privilege/platform skips: native 30 passes/two symlink skips, Python 28
passes/four skips. The fresh MSVC 19.51 Debug build passed. No engine campaign was
required or claimed for these inert semantic changes.

All 75 selected historical hashes matched; DESKTOP_UX was verified in the versioned
baseline because of the preexisting local overlay. Four intentionally changed
source/build files were separately checked against their historical hashes in that
baseline. All ten current audited source hashes and all twelve preexisting local
files were checked. Workspace checker: 517 text files passed; `git diff --check`
passed. Exact publication-tree validation is recorded separately below.

The first exact staged-tree checker passed for 515 text files. Publication review
found CRLF in the newly generated probe, while Git requires LF. Only that new
source's line endings were normalized; its rebuilt probe and both focused CTest
entries passed again (22 cases/157 comparisons). The
[additive publication audit](evidence/SN-021-bindings-publication.json) records
the final ten source hashes and binary hashes, retaining the initial audit and
CRLF hash unchanged. The extra evidence file adds one to both checker totals.
Final exact staged-tree checker: 516 text files passed; workspace checker:
518 passed. All ten final source hashes match both trees; whitespace checks pass.

All twelve preexisting SN-045 documentation changes remain local and excluded
from publication, including their shared planning edits. The published tree
therefore excludes two local-only documents. The final publication record identifies
source and hosted verification separately from local acceptance.

This is declarative interface validation, not actual SVG/SPICE interface evidence
or engine integration. No resource is opened, rendered or executed by the API.
The developer probe consumes bounded stdin and emits validated IDs/quantities;
it does not select a public project or automation protocol. Physical Windows/NTFS
resource verification retains its own containment, alias and replacement rules.

Resource lock/link and full project semantics remain pending natively, followed
by source graph loading. Actual source interfaces, trust/redistribution, firmware/
boot, atomic saving, compilation and runtime authorization remain separate gates.
SN-044, MCU/toolchain independence, shared instrumentation, accepted numerical
profiles, PDF/PID fixes and SN-017 Python/GDB fixture ownership remain unchanged.
Allocation failure is structured but not fault-injected; no hard memory/wall quota
is claimed. Native lone-surrogate rejection remains stricter than Python.

## Reproduction

```powershell
cmake -S . -B build/sn021-bindings-host
cmake --build build/sn021-bindings-host --config Debug
ctest --test-dir build/sn021-bindings-host -C Debug --output-on-failure --verbose
python tools/check_repository.py
git diff --check
```
