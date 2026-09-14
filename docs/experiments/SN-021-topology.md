# SN-021 native topology semantic validation

Date: 2026-09-14. Status: bounded semantic implementation; final results below.
Base main: `386fef7586b8fa4ec34dbd6bddc559a109f27800` (PR #22 squash).

[ADR 0056](../decisions/0056-native-topology-semantics.md) and the
[native contract](../architecture/NATIVE_TOPOLOGY_VALIDATION.md) define this
version-specific step. It implements the complete preserved topology 0.1 semantic
rules over owned native syntax. It does not downgrade/project topology 0.3, load
a full project, publish an editable graph or open resources. Later versions and
unknown fields are rejected. Names do not create implicit connections.

## Acceptance coverage

The unchanged Python topology suite is reused through a differential wrapper;
native statistics/error categories must match its original parse/validate calls.
Additional cases exercise exact 4096-entity, eight-level and 16384-occurrence
limits, overflow, Unicode scalar name lengths/control rejection, namespace
collisions, generated DAGs with permuted definition order and nested field/type
mutations. The native C++ test checks immutable source ownership after caller
mutation/result release and the exact error offset for a missing root reference.

The initial 21-case focused suite passed 147 native/reference comparisons. Nested
field mutations subsequently increased coverage. These synthetic structural tests
are appropriate to pure semantic validation; they are not engine integration
or independent electrical-state evidence. No historical fixture/test is rewritten.

## Local-work preservation

Final local [audit](evidence/SN-021-topology-summary.json): 22 topology test cases,
316 native/reference comparisons and native ownership/source-offset checks passed.
All 22 Windows CTest entries passed, including the unchanged 94 schema tests,
95 syntax assertions and 209 syntax differential cases. Native resource checks
retain 30 local passes/two symlink privilege skips; Python resource checks retain
28 passes/four skips. Configure/build passed with MSVC 19.51 and SDK 10.0.26100.0.
No semantic test/build failure occurred in this slice.

All 62 selected historical hashes matched. One (DESKTOP_UX) was checked in the
filtered base checkout because its preexisting SN-045 overlay intentionally
differs; that overlay was separately checked against its exact preserved bytes.
The audit records this distinction, source/binary hashes and full CTest output.
Workspace checker: 499 text files passed; `git diff --check` passed. The published
tree excludes two local-only SN-045 documents and is checked separately.
The exact staged publication tree passed the checker for 497 text files. Its
shared planning files contain only SN-021 additions; staged whitespace checks
passed. This distinguishes published acceptance from the preserved local overlay.

Twelve preexisting SN-045 documentation files were found before this slice.
Their exact bytes and original diff were preserved under ignored build storage.
Only SN-021 additions to shared CURRENT/BACKLOG are staged; the SN-045 content
remains local and is excluded from this PR. Repository checking distinguishes
the published tree from the workspace containing those two extra local documents.
No new automation/UI work is included or authorized by this slice.

## Reproduction and limits

Source `4cccfe7` was pushed on `codex/sn-021-topology-semantics`.
[PR #23](https://github.com/RicardoKers/SimNodus/pull/23) records final required
Foundation checks and authorized protected-main squash integration. Windows
runs 22 CTest entries and Linux 17, including the two new topology entries.
Consult actual logs for results; job expectations are not acceptance evidence.

```powershell
cmake -S . -B build/sn021-topology
cmake --build build/sn021-topology --config Debug
ctest --test-dir build/sn021-topology -C Debug --output-on-failure --verbose
python tools/check_repository.py
git diff --check
```

The test-only probe reads bounded stdin, never a resource path. Application code
does no I/O and exports no public CLI/automation contract. Input remains bounded
to 1 MiB/32 containers; native ingress rejects lone surrogates more strictly than
the historical Python parser. Allocation failure is structured but not injected;
no hard OS memory/wall quota is claimed. Diagnostic ordering is not a public ABI.

Next port exact parameter/scoped-override semantics, followed by descriptor,
resource and project rules, before graph publication. Current full declaration
validation remains Python-owned. Physical verification, source interfaces, trust,
redistribution, firmware/boot and runtime approval remain independent. Saving,
compilation, UI, engine profiles/tolerances, PDF/PID fixes and SN-017 Python/GDB
fixture ownership remain unchanged. SN-021 stays in progress.
