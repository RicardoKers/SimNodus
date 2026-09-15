# SN-021 structural connectivity compilation acceptance

Date: 2026-09-15. Base main: `3b377e39ecb76f9941cc64872c23e6b0509ad513`.
Status: structural lowering only; full SN-021 remains in progress.

[ADR 0067](../decisions/0067-connectivity-compilation.md) and the
[contract](../architecture/CONNECTIVITY_COMPILATION.md) define the selected scope,
limits and readiness separation. The [audit](evidence/SN-021-connectivity-compilation-summary.json)
records commands, outcomes, source/binary hashes and historical preservation.

## Acceptance

Ten focused cases and 18 requests passed. An independent Python implementation
expands the validated definitions into adjacency lists and uses graph traversal
to find connected groups; native code uses union-find. Results compare every
occurrence and group, all terminal/net source spans, canonical tuple ordering and
exact retained source bytes. Native output is read after releasing input and its
original result wrapper, retaining a shared compilation copy.

The two-RC fixture produces four connectivity classes while keeping left/right
output occurrences distinct. Tests also cover an empty root, all 18 disconnected
terminal singletons, misleading `GND` labels/directions, parameter changes without
rewiring, source array reordering, unused definitions, nested port aliases and
invalid full declarations. The operation accepts exactly 65536 expanded records
(257 occurrences and 65279 disconnected terminals). Adding one port produces an
expansion-budget error while the unchanged reference still accepts the declaration.
All source metadata, numeric spelling and definition identity remain intact.

## Reproduction and limits

The audit verified 154 historical hashes and the previous CMake hash in the base.
Eight current source hashes are recorded. All twelve preexisting SN-045 changes
remain local and excluded, including shared planning edits. The repository checker
passed for 603 workspace text files; historical evidence files were not rewritten.

Configuration and build passed with MSVC 19.51 and Windows SDK 10.0.26100.0.
All 40 Windows CTests passed, including 94 schema tests and prior source graph,
project, revision, acquisition, resource, save and overwrite regressions. The new
structural suite has no skips; prior physical privilege skips remain explicit in
the audit. No configuration, compilation or test failure occurred in this slice.

```powershell
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --verbose
python tools/check_repository.py
```

No resource/model parser, backend netlist, solver node allocation, reference-ground
selection, numerical parameter translation or execution readiness is implemented
by this stage. No file/resource is opened and no engine integration claim follows.
The declared SPICE interface still requires verification against captured bytes;
fixture model text does not establish an electrical profile. Real-engine evidence
is required once a subsequent stage emits/consumes backend inputs.

Next select and verify the smallest model/interface and parameter lowering contract,
including explicit reference/stimulus/analysis authority, within existing supported
profiles. Safe overwrite remains pending ADR 0064. Preserve all previous evidence,
local SN-045 work, SN-044, numerical tolerances, MCU/toolchain independence, PDF/PID
and the SN-017 Python/GDB/fixture boundary. No UI or other SN implementation is
selected. Prepare the next-cycle prompt only when full SN-021 is accepted.
