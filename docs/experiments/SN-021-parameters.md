# SN-021 native exact parameters and scoped overrides

Date: 2026-09-14. Base main: `f05b993a278e2372f97aa62bda3277b7703e26bc`.
Status: bounded topology 0.2 implementation; SN-021 remains in progress.

[ADR 0057](../decisions/0057-native-exact-parameters.md) and the
[contract](../architecture/NATIVE_PARAMETERS.md) define exact quantity validation,
scoped interval-safe forwarding and immutable per-occurrence inspection. Original
syntax/source positions remain owned. Structural validation is reused internally
without rewriting/projection or weakening the public topology 0.1 entry point.
This does not implement full project loading, an editable graph or a public CLI.

## Acceptance coverage

The unchanged Python parameter suite is wrapped to compare native snapshots,
formatted values, units, origins, structural statistics and rejection categories.
Direct reference quantity/number checks are also exercised through native fixture
documents. Additions cover generated decimals across the closed unit vocabulary,
exact precision/exponent boundaries, signed zero, negative range neighbors,
unreachable invalid declarations, names, use-site origins and exact budgets.

The first focused run passed 20 cases/311 comparisons. The expanded focused run
passed 22 cases/315 comparisons, adding the exact combined declaration/override
limit and forwarded-use-site origin checks. Native C++ checks passed for ownership
after caller mutation/result release, independent pin/parameter namespaces,
negative zero, version isolation, and exact default/literal/forwarding/error offsets.
No failed build or test run occurred in those focused checks.

These are inert declaration/inspection tests, not engine integration or proof
of electrical behavior. No resource is opened, interpreted, rendered or executed
by the API. The probe has bounded developer-only stdin and validated ASCII output;
it is not a project format or public automation API.

## Preservation and remaining gates

The final local [audit](evidence/SN-021-parameters-summary.json) passed all
24 Windows CTest entries: 22 parameter cases/315 comparisons, native ownership/
offset checks, the unchanged topology suite (22 cases/316 comparisons), 95 syntax
assertions, 209 syntax differential cases and 94 original schema tests. Native
resource checks retained 30 local passes/two unavailable symlink privileges;
Python resource checks retained 28 passes/four skips. The full Debug build passed
with MSVC 19.51/Windows SDK 10.0.26100.0. These are not new engine-profile results.

All 68 selected historical hashes matched; DESKTOP_UX was checked against the
versioned baseline because its preexisting SN-045 overlay intentionally differs.
The old topology implementation hash was independently verified in that baseline,
and its updated hash is recorded as a new source. No old evidence is overwritten.
The audit also records commands, source/local binary hashes and full CTest output.
Workspace checker: 509 text files passed; `git diff --check` passed. The exact
publication tree is checked separately because it excludes two local SN-045 files.
The exact staged tree passed the repository checker for 507 text files and
staged whitespace checks passed. Its shared planning files contain only SN-021
additions; the SN-045 overlay remains independently preserved in the workspace.

The twelve preexisting SN-045 documentation changes are preserved locally and
excluded from this publication, including their shared planning edits. Historical
evidence remains unchanged. The topology implementation is deliberately extended
for the internal 0.2 structural phase; prior hashes retain their historical meaning
and the original topology regression suite remains a required gate.

Full topology 0.3 descriptor/model rules, resource lock/link and project semantics
remain pending natively. Next port descriptor/model declaration validation before
resource/project semantics and graph loading. Physical containment and snapshots,
actual source interfaces, trust/redistribution, firmware/boot and runtime approval
remain independent gates. Saving, compilation, UI, engine profiles/tolerances,
PDF/PID behavior and SN-017 Python/GDB fixture ownership remain unchanged.

Allocation failure is structured but not fault-injected; no hard memory/wall
quota is claimed. Native ingress retains stricter lone-surrogate rejection than
historical Python. Exact decimal arithmetic here is bounded by this schema and
is not a general-purpose numeric library or expression evaluator.

## Reproduction

Source `a561149` was pushed on `codex/sn-021-native-parameters`.
[PR #24](https://github.com/RicardoKers/SimNodus/pull/24) records final required
Foundation checks and authorized protected-main squash integration. Windows
runs 24 CTest entries and Linux 19, including both new parameter tests. Consult
the final logs for actual results; expected job contents are not acceptance.

```powershell
cmake -S . -B build/sn021-parameters
cmake --build build/sn021-parameters --config Debug
ctest --test-dir build/sn021-parameters -C Debug --output-on-failure --verbose
python tools/check_repository.py
git diff --check
```
