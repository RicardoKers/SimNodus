# SN-020 declaration baseline acceptance

Date: 2026-09-13. Result: **done for the circuit/component declaration schema and
reference validation baseline**, under [ADR 0051](../decisions/0051-project-declaration-baseline.md).
The accepted deliverable is [project declarations 0.1](../architecture/PROJECT_SCHEMA.md).
It is suitable as the official development specification, not an executable
application format or a claim that arbitrary declared circuits can run.

## Acceptance mapping

| Required contract | Implemented specification and evidence | Remaining implementation gate |
|---|---|---|
| Project/entity identity and independent graph | Project ID; component/pin/net/port IDs; checked hierarchy and distinct occurrence paths; preserved round trips | Native graph loading, editing, migration and atomic saving |
| Parameters/overrides | Exact decimal quantities, dimensional/range checks, independent instance values | Runtime compilation and backend parameter translation |
| Symbol/layout/model separation | Separate descriptor catalogs, pin/parameter maps, typed resource links, explicit model entrypoints; visual rebinding preserves graph/models | SVG anchors/geometry, rendering and actual source-interface validation |
| Locked resources and provenance | Exact inventory versions, per-file hashes, inventory digest, origin/license/notice references, bounded lexical paths | Same-handle physical containment and bytes; actual origin/redistribution review |
| Board/MCU/firmware metadata | Separate board/MCU identity, declared architecture, locked ELF reference, per-occurrence targets and complete pin maps; reject double model ownership | ELF/boot/memory-map/platform compatibility and real backend/device evidence |
| Temporal/fidelity configuration | Distinct virtual ns and wall ms, fixed numerical bounds, explicit approximation and fail-closed unsupported requests | Exact profile negotiation, schedule contents and executable session preparation |
| Safe input/version handling | Bounded UTF-8 JSON, duplicate/unknown fields rejected, no migration or implicit execution, structured errors | OS-specific resource loading and execution isolation |

All **94 schema tests passed**: 64 prior, 15 resource-link tests and 15 project
tests. The saved two-RC project preserves the embedded source document, contains
no MCU target and has explicit unconfigured timing. Synthetic in-memory target/
firmware/schedule declarations exercise successful association, missing/duplicate
paths, architecture mismatch, ownership conflicts and policy errors. They are
not real firmware or fake-backend integration evidence. Owned SVG/SPICE fixture
bytes are verified in separate trusted-file tests; no rendering/importing occurs.

The native Debug build succeeded. All **15 CTest entries passed**: the schema
suite plus 14 existing native invariant tests. CI now builds the default foundation
and runs schema/native foundation tests on both hosted platforms. Hosted success
is a separate publication gate, not inferred from this local run. No Renode,
ngspice or CubeIDE integration campaign was rerun for declaration-only changes.

A fresh default configure/build in `build/sn020-foundation-01` also passed,
followed by all 15 CTest entries and the CMake repository-check target. The first
CTest call in that fresh directory ran prematurely while compilation was still
active, so 11 executables were not yet available; after the build completed,
the full run passed without a source change. This was command sequencing, not
a passed initial attempt or an engine failure. The repository checker passed
for 459 text files and `git diff --check` passed.

The valid project CLI returns `valid-project-declarations-only` with five linked
descriptors, two assets, three resources and zero targets. Every resource,
firmware, physical-containment, runtime-profile and simulation-readiness flag is
false. The invalid role CLI exits 1 with `kind`. Prior schema evidence hashes are
rechecked in the [machine-readable evidence](evidence/SN-020-acceptance-summary.json).

```powershell
python -m unittest discover -s tests/schema -p 'test_*.py'
python tests/schema/project.py tests/schema/fixtures/two-rc-project.json
python tests/schema/resource_links.py tests/schema/fixtures/invalid-resource-kind.json
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tools/check_repository.py
git diff --check
```

## Scope closure and preservation

SN-020 specifies and validates declarations; it does not implement SN-021's
physical loader, atomic saver or compiler. The accepted baseline deliberately
rejects unspecified extensions rather than admitting unchecked executable fields.
New categories, schema migrations, layout/probe data and package discovery need
versioned contracts in their existing implementation tasks. UI/UX SN-044, ADRs
0027/0028/0049 and their pending decisions remain intact. No other SN is advanced.

Original drafts and failed/inconclusive historical engine evidence remain
preserved. Python still owns preparation, GDB transport and fixture sequencing
for accepted SN-017 composition. Tolerances, PDF suppression and PID retry are
unchanged. Runtime support remains bounded by earlier engine evidence, not by
the broader vocabulary of these metadata declarations.

The owner authorized coherent validated commits and integration through the
existing protected-main pull-request/squash flow. The acceptance baseline, its
tests and preserved accepted documentation are the intended integration unit;
historical experimental drafts remain labelled as such. This does not publish
an experimental runtime, binary release or new supported simulation profile.
Publication status is recorded in CURRENT and must be confirmed from GitHub.
