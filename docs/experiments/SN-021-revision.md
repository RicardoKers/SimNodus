# SN-021 project display-name revision acceptance

Date: 2026-09-15. Base main: `8a60f6bc870b2a277749d24f309204cca2dbed7f`.
Status: minimal source-preserving revision; SN-021 remains in progress.

[ADR 0066](../decisions/0066-project-name-revision.md) and the
[contract](../architecture/PROJECT_REVISION.md) define the selected step and
acceptance criteria. The [audit](evidence/SN-021-revision-summary.json) records
commands, results, source/binary hashes and historical preservation.

The pure native operation changes only a validated project's display-name token.
It revalidates/rebuilds the graph and provenance, preserving every other source
byte and stable ID. A semantic no-op preserves all original spelling. No source
file, resource or simulator is opened and no pathname/save authority is granted.

## Acceptance

Eleven focused cases and 33 revision requests passed. An independent Python JSON
decoder locates the original token; expected output preserves its exact prefix
and suffix. The unchanged reference validator checks complete revised metadata,
and graph expectations check connectivity, every source-map identity and every
mapped byte span. The native probe releases input strings and its original result
wrapper before serializing its retained graph copy.

Cases cover quotes/backslashes/Unicode, JSON-looking name content, 80 Unicode
scalars, escaped keys, semantic no-ops with original escape spelling, reordered
top-level fields, repeated edits, invalid names/UTF-8/control characters, invalid
base rejection and exact 1 MiB no-op/shrink/growth boundaries. An escaped original
can shrink even when the replacement's decoded name is longer. No ID, topology,
numeric value, resource descriptor or unrelated metadata is changed.

## Reproduction and limits

The audit verified 147 historical hashes and the previous CMake hash in the base.
Seven current source hashes are recorded. All twelve preexisting SN-045 changes
remain local and excluded, including shared planning edits. The repository checker
passed for 594 workspace text files; historical evidence files were not rewritten.

Configuration and build passed with MSVC 19.51 and Windows SDK 10.0.26100.0.
All 39 Windows CTests passed, including 94 schema tests and prior graph, project,
resource, acquisition, save and overwrite regressions. No configuration, build
or test failure occurred in this slice. Existing physical privilege skips are
retained explicitly in the audit; the new pure revision suite has no skips.

```powershell
cmake -S . -B build/sn021-save
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --verbose
python tools/check_repository.py
```

This is a bounded value transformation on Windows and Linux, not a generic patch
API, circuit editor, history store or concurrent session transaction. The existing
limits bound input/candidate bytes and validation entities; no hard OS memory/time
quota or allocation-failure injection is claimed. Historical physical-test privilege
skips remain recorded. No real engine is needed for a pure display-name edit;
these tests do not validate compilation or simulation integration.

Next define the minimal compiler ingress/readiness and lowering contract with
stable source mappings under existing profiles, then obtain required real-engine
evidence. Safe overwrite remains pending ADR 0064. Further editor features are
not selected. Prepare the requested next-cycle prompt only after full SN-021.
