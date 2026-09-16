# SN-021 exact passive numerical binding

Date: 2026-09-15. Status: accepted bounded slice; full SN-021 in progress.
Base main: `8e4c3e2850bc73753fd3a600676c0a235a351b09` (PR #36).
Branch: `codex/sn-021-passive-numeric`.

The [contract](../architecture/PASSIVE_NUMERIC_BINDING.md) and
[ADR 0070](../decisions/0070-passive-numeric-binding.md) define the acceptance gate.
The new operation reruns interface validation, converts the selected source default
exactly, enforces positive inclusive range membership, and binds reachable component
values/pins to source order with stable paths and complete provenance. Defaults
are checked even when overridden or unused. Output retains the original metadata
and source; readiness flags remain false. No I/O, netlist or engine call is added.

Eight regression cases passed, including 40 suffix/exponent comparisons against
Python Decimal at precision 100, inclusive and adjacent range boundaries,
32-digit effective values, inherited/component defaults, zero/negative rejection,
reversed source maps, metadata reordering, unused descriptors and delegated errors.
The preexisting Python resolver independently checks inherited values and origins.
All 43 Windows CTests passed; prior platform/privilege skips remain explicit in
the [audit](evidence/SN-021-passive-numeric-summary.json). No new test skip.

```powershell
cmake --build build/sn021-save --config Debug
ctest --test-dir build/sn021-save -C Debug --verbose
python tools/check_repository.py
```

No engine behavior changes: prior [real RC evidence](SN-021-passive-source.md)
is preserved, not replaced by these mathematical tests. Exact binding does not
certify solver rounding, stability or a wider electrical profile. Positive R/C
requirements restrict this operation without changing declaration validity or
accepted tolerances. All historical hashes and twelve local SN-045 files are
preserved. Next define explicit reference/stimulus/analysis authority and bounded
backend lowering with complete project/physical gates and real-engine acceptance.
Safe overwrite remains pending ADR 0064. Full SN-021 stays **in_progress**; prepare
the next-cycle prompt only after full acceptance and integration.
