# SN-021 native source graph acceptance

Date: 2026-09-15. Base main: `d78c7bef598b6fe65d300584c1da3709d84af1b7`.
Status: immutable source definition graph loaded; SN-021 remains in progress.

[ADR 0062](../decisions/0062-native-source-graph.md) and the
[contract](../architecture/NATIVE_SOURCE_GRAPH.md) define standard C++ connectivity
values, complete retained project metadata and separate graph provenance. The
[audit](evidence/SN-021-graph-summary.json) records environment, commands, results,
source/binary hashes and historical preservation. Reference validators are unchanged.

## Acceptance

All 20 focused cases passed, with 56 native/reference comparisons. Each accepted
case compares every graph record with an independent connectivity view of the
Python-validated project. Every mapped byte span is decoded and compared with the
corresponding full original declaration object, including metadata. Mapping keys
are unique and cover all graph records, root selection and terminal references.

Tests include repeated scoped instances, reordered arrays/keys, duplicate GND
labels with Unicode/quotes/backslashes, disconnected ports, unreachable definitions,
an empty graph and the exact 4096-entity boundary. Symbol/model metadata changes
do not rewire connectivity. Invalid connections, unused definitions and complete
project metadata reject the entire operation without graph output. Native tests
confirm immutable ownership after input/result release, independent domain copies,
distinct source positions and original nested error offsets.

The MSVC 19.51/SDK 10.0.26100.0 Debug configuration/build passed. All 34 Windows
CTests passed, including the prior 106 project, 57 resource-link, 135 lock/137 digest,
157 binding, 315 parameter and 316 topology comparisons, syntax checks, 94 original
schema tests and foundation contracts. Native physical tests retain 30 passes/two
symlink privilege skips; Python resource tests retain 28 passes/four explicit skips.
Hosted Foundation checks must report their actual Windows/Linux acceptance before
protected-main integration; synthetic graph tests make no engine integration claim.

All 113 selected historical hashes matched; the changed CMake source hash was
verified in the base. DESKTOP_UX was checked in the base due to preexisting local
work. All ten audited source hashes match the workspace. Twelve preexisting SN-045
changes remain local and excluded, including shared planning edits.
The repository checker passed for 559 workspace and 557 publication-tree text
files. All ten source hashes match both trees; staged whitespace and shared
planning review passed. The publication excludes the two local-only documents.

The initial patch could not create the new domain directory under workspace
permissions. Creating that directory with authorized host access allowed the patch
to apply. No configuration, compilation or test failure occurred in this slice.
The audit preserves the initial file-creation failure and all earlier evidence.

## Limits and next step

The graph preserves source hierarchy; it is not a flattened simulation graph,
solver-ready netlist or editing transaction API. Parameters, resource bindings,
platforms and temporal policies remain in the complete owned declaration. No
field is dropped from the retained source. Source-map keys identify definitions
and endpoints; expanded occurrence paths remain in the parameter snapshot.

No file/resource is opened by the loader. Physical containment/byte verification,
source/ELF/boot interfaces, trust, redistribution and execution approval remain
independent gates. All inherited readiness flags stay false. No resource rendering,
DLL/model/firmware loading, download, engine execution, ground inference, schema
migration or supported-profile expansion is performed.

Next implement bounded safe persistence of owned validated source with destination
ownership, atomic replacement and failure-preservation tests, then compilation
with stable source mappings and required real-engine evidence. Path-based project
acquisition and editing remain separate work. Preserve SN-044, local SN-045, shared
instrumentation, MCU/toolchain independence, numeric bounds, PDF/PID behavior and
SN-017 Python/GDB/fixture ownership. Prepare the next-cycle chat prompt only after
full SN-021 acceptance is integrated. Allocation failure is not fault-injected;
native Unicode remains stricter than historical Python and hard OS quotas are not
claimed. Domain values are not security capabilities for later consumers.

## Reproduction

Source `c66ff52` was pushed on `codex/sn-021-source-graph`.
[PR #29](https://github.com/RicardoKers/SimNodus/pull/29) records final required
Foundation checks and authorized protected-main squash. Expected jobs contain
34 Windows and 29 Linux CTests; consult final logs for actual hosted acceptance.

```powershell
cmake -S . -B build/sn021-graph
cmake --build build/sn021-graph --config Debug
ctest --test-dir build/sn021-graph -C Debug --output-on-failure --verbose
python tools/check_repository.py
git diff --check
```
