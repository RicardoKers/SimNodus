# SN-020 second slice: exact parameters and instance overrides

Date: 2026-09-13. Status: slice passed; **SN-020 remains in progress**.
[ADR 0046](../decisions/0046-exact-parameter-draft.md) records the
[experimental 0.2 parameter contract](../architecture/PARAMETERS_DRAFT.md).

## Implemented and observed

The developer reference validator adds declaration defaults/inclusive bounds,
exact decimal-string quantities with a closed unit vocabulary, literal overrides
and explicit containing-circuit parameter forwarding. It checks full source
interval containment for forwarded parameters, not only current defaults.
Instance-local resolution produces an inspection snapshot without changing
shared definitions. A separate expanded instance/value budget prevents a small
parameterized graph from multiplying into excessive output.

All **29 tests passed**: 15 unchanged topology tests plus 14 parameter tests.
Coverage includes decoded-tree round trip and source spelling, distinct RC
instance values, immutable definitions, exact unit conversion under reduced
ambient decimal precision, inclusive boundaries, unsafe forwarded ranges,
dimension mismatch, unknown units/parameters, numeric budgets, rejected
expressions/JSON numbers, version separation and resolved-value expansion limits.

The CLI accepted the 0.2 fixture with 26 topology entities, nine parameter/
override entries, six instance occurrences and 14 expanded instance/value entries.
Left leaf values resolved to 1000 ohm and 1e-6 F; right to 2200 ohm and 220e-9 F.
The invalid-dimension fixture exited 1 with the expected `unit` error. These
values are document inspection results, not SPICE outputs or electrical proof.
All hashes recorded by the 0.1 evidence still matched, including its validator,
tests, fixtures, contract and ADR. See [evidence](evidence/SN-020-parameters-summary.json).
The repository checker passed (426 text files), and `git diff --check` passed.

```powershell
python -m unittest discover -s tests/schema -p 'test_*.py' -v
python tests/schema/parameters.py tests/schema/fixtures/two-rc-parameters.json
python tests/schema/parameters.py tests/schema/fixtures/invalid-parameter-dimension.json
python tools/check_repository.py
git diff --check
```

## Limits and next step

0.2 is a separate draft; no 0.1 migration or input rewrite is performed. Units
are exact, closed tokens, not a generic SPICE/SI parser. Root runtime overrides,
arithmetic expressions and affine conversions are unsupported. Valid topology
and parameters do not make a circuit simulable; no model or backend is selected.

Next specify **separate declarative symbol/model descriptors with explicit pin
and parameter mappings**, validating identifiers and references without opening
external resources or loading code. Dependency/resource containment, board/MCU/
firmware and temporal configuration remain pending; SN-021 loading/saving and
compilation stays planned. Existing engine evidence, numerical tolerances, PDF
suppression, PID retry, Python GDB/fixture ownership and ADRs 0027/0028 are unchanged.
No engine execution, commit, issue synchronization or publication occurred.
