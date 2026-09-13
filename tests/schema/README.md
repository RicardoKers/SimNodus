# SN-020 topology draft fixtures

The [0.3 descriptor draft](../../docs/architecture/BINDINGS_DRAFT.md) adds separate
symbol/model interfaces and mappings. All 46 schema tests run with
`python -m unittest discover -s tests/schema -p 'test_*.py' -v`.
Use `python tests/schema/bindings.py tests/schema/fixtures/two-rc-bindings.json`
for the valid 0.3 fixture; `invalid-model-alias.json` must exit 1 with `mapping`.
See the [binding report](../../docs/experiments/SN-020-bindings.md). Older draft
instructions below retain their own version-specific scope and test counts.

The separate [0.2 parameter draft](../../docs/architecture/PARAMETERS_DRAFT.md)
adds exact quantities and instance overrides while preserving 0.1 files below.
Run all 29 tests with `python -m unittest discover -s tests/schema -p 'test_*.py' -v`.
Validate the new fixture with
`python tests/schema/parameters.py tests/schema/fixtures/two-rc-parameters.json`;
`invalid-parameter-dimension.json` must exit 1 with `unit` when passed to that CLI.
The [parameter report](../../docs/experiments/SN-020-parameters.md) records scope/results.

These owned fixtures validate the [0.1 topology draft](../../docs/architecture/TOPOLOGY_DRAFT.md).
They do not load firmware, instantiate electrical models or run a simulation.

```powershell
python -m unittest discover -s tests/schema -p test_topology.py -v
python tests/schema/topology.py tests/schema/fixtures/two-rc.json
python tests/schema/topology.py tests/schema/fixtures/invalid-cycle.json
python tests/schema/topology.py tests/schema/fixtures/invalid-terminal.json
```

The valid fixture exits 0 with 26 declared entities, root depth 2 and six expanded
instance occurrences. Invalid-cycle exits 1 with `hierarchy`; invalid-terminal
exits 1 with `reference`. These failures are expected. Fifteen tests cover JSON
round trip, distinct repeated-instance paths, order/name independence, reference
integrity, unknown execution fields, malformed input, recursion and resource
budgets. Input files are never modified.

The validator intentionally rejects layout/model/firmware/resource fields until
they have their own specified contracts. No topology-valid document is promoted
to simulator-ready. Keep this draft and its evidence when evolving the version.
