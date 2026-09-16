# SN-021 bounded passive source inspection

Date: 2026-09-15. Status: accepted bounded slice; full SN-021 in progress.
Base main: `de8e1438cf9b51101465d64f83d5e0451e8777f3` (PR #34).
Branch: `codex/sn-021-passive-source`.

## Implementation and acceptance

The [contract](../architecture/PASSIVE_SPICE_SOURCE.md) and
[ADR 0068](../decisions/0068-passive-source-inspection.md) define the source reader
and predeclared integration criteria. The reader owns accepted bytes, ordered
formal interfaces and source spans. It rejects commands, includes, additional
devices, nested subcircuits, expressions, unsupported encodings and over-budget
input. It performs no I/O and grants no readiness or execution authority.

Seven regression cases passed, including exact byte/line/model budgets, case
collisions, reserved parameters, unsupported constructs and input lifetime.
All 41 Windows CTests passed. Existing platform/privilege skips remain explicit
in the [audit](evidence/SN-021-passive-source-summary.json); no new reader skip.

Explicit integration captured the unchanged owned fixture using native retained
handles and its existing lock, inspected the captured bytes, then supplied those
same bytes to an isolated E-01 run with the nine verified pinned ngspice files.
There were no downloads. The harness explicitly supplied ground, stimulus,
initial condition, parameter values and analysis; no project was executed.
The successful run produced 5012 samples through 5 ms, maximum analytical error
`9.889724283951296e-08 V`, below unchanged `0.0165 V`. Endpoint tolerance remains
`1e-12 s`. Callback values matched final vectors; process and idle shutdown passed.

## Preserved failed attempts

Initial CMake configuration failed due to a duplicate new target block; it was
removed. A sandbox MSBuild invocation failed due to duplicated `Path`/`PATH`
environment keys; the authorized normal build then passed.
The first engine attempt passed analytical/shutdown checks but the harness
incorrectly compared entire CSV dictionaries: callbacks include an `index`
column absent from final vectors. The harness now compares sample counts and
the same three numerical columns used by E-01. The second run passed without
changing engine inputs, numerical tolerances or historical fixture bytes.
Both attempt results and raw hashes are retained in the audit; the first is
still labelled failed. Raw outputs remain under ignored `build/` directories.

## Reproduction and limits

```powershell
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --verbose
python tests/experiments/ngspice/passive_source_acceptance.py --output build/sn021-passive-new-run
python tools/check_repository.py
```

Engine reproduction requires the existing E-01 executable and pinned local
dependencies. The output directory must be new. CI runs the inert tests without
native engine dependencies; local integration proves only this owned ideal RC
fixture, not every literal accepted by the grammar or arbitrary SPICE models.
No implicit interface flag, source trust, redistribution permission, project
compiler, numerical parameter binding or destination overwrite is implemented.
Next bind verified recognized models to declared ordered maps, units/ranges and
effective parameters, with explicit reference/stimulus/analysis authority.
Preserve SN-017 and all accepted profiles. Full SN-021 remains **in_progress**;
the next-cycle chat prompt is pending full acceptance.
