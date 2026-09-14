# SN-021 native resource-link acceptance

Date: 2026-09-14. Base main: `a6daf1d2ccc3bf409a45b479407b276d7b1e2690`.
Status: bounded resource-links 0.1 implementation; SN-021 remains in progress.

[ADR 0060](../decisions/0060-native-resource-links.md) and the
[contract](../architecture/NATIVE_RESOURCE_LINKS.md) define typed asset roles,
complete explicit bindings and one shared immutable syntax capture. Internal
subtree entry points preserve original absolute source positions. Standalone
topology, parameter, binding and lock version gates remain strict.

## Focused acceptance

The fresh MSVC 19.51/Windows SDK 10.0.26100.0 Debug build passed. All 19 link cases
passed: the unchanged 15-case Python reference suite and four added adversarial
cases, with 57 native/reference comparisons of complete statistics and rejection
categories. Exact combined/256-asset boundaries, unused assets, case-insensitive
source aliases, token limits, independent namespaces, null/empty associations
and nested unknown fields are covered. No failed build/test run was observed.

C++ tests passed for shared capture ownership after caller mutation/result release,
nested path error offsets and retained file positions. Each parameter occurrence's
path, definition, value, unit, origin and translated source offset matches the
standalone binding result. The nested results retain the same syntax pointer.
Trusted fixture reads occur explicitly in the test host, never in the API.

## Full regression and remaining gates

The [audit](evidence/SN-021-links-summary.json) records final local regression,
source/binary hashes and historical input verification. The publication record
below identifies final hosted checks and protected-main integration separately.
All twelve preexisting SN-045 documentation changes remain local and excluded,
including their CURRENT/BACKLOG overlay. Older evidence is never overwritten.

All 30 Windows CTest entries passed: 57 resource-link comparisons, 135 lock
comparisons/137 digest cases, 157 binding comparisons, 315 parameter comparisons,
316 topology comparisons, 95 syntax assertions, 209 syntax differential cases and
94 original schema tests. Native physical resources retained 30 local passes/two
symlink privilege skips; Python resources retained 28 passes/four explicit skips.
All 92 selected historical hashes matched; five changed source/build hashes were
separately verified in the base. DESKTOP_UX was checked in the baseline because
of preexisting local work. All thirteen audited source hashes match the workspace.
Repository checker: 539 text files passed; whitespace checks passed. The exact
publication tree excludes the two local-only SN-045 documents.
Its checker passed for 537 text files. All thirteen source hashes match both the
workspace and publication tree; staged whitespace and shared planning review passed.

Declaration association is not actual SVG/SPICE interface verification, rendering
safety, source trust, redistribution or execution authorization. The API never
opens resources, downloads dependencies, loads DLLs/models/firmware, compiles or
starts engines. Existing physical handle/alias/reparse/race protections remain
unchanged. No new engine profile or integration claim follows from these tests.

Next compose project declarations, then native source graph loading, safe saving
and source-preserving compilation with their own acceptance evidence. Actual
source interfaces and runtime/device compatibility remain independent gates.
SN-044, local SN-045 work, instrumentation, MCU/toolchain independence, numerical
profiles, PDF/PID fixes and SN-017 Python/GDB ownership remain unchanged. Native
Unicode is stricter than historical Python; allocation failures are structured
but not fault-injected, and hard OS memory/wall quotas are not claimed.

## Reproduction

```powershell
cmake -S . -B build/sn021-links
cmake --build build/sn021-links --config Debug
ctest --test-dir build/sn021-links -C Debug --output-on-failure --verbose
python tools/check_repository.py
git diff --check
```
