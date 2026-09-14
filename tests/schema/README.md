# SN-020 topology draft fixtures

SN-021 adds [native exact parameters](../../docs/experiments/SN-021-parameters.md).
CTest runs `native-parameter-contracts` and `native-parameter-differential`, comparing
the unchanged Python parameter reference with native inspection values and errors.
This does not open resources or select a public CLI contract.

SN-021 adds [native topology semantics](../../docs/experiments/SN-021-topology.md).
CTest runs `native-topology-contracts` and `native-topology-differential`; the
latter reuses the unchanged topology reference tests and adds bounded adversarial
cases. The developer probe is not a public CLI or full project validator.

SN-021 adds separate [native syntax ingress tests](../../docs/experiments/SN-021-ingress.md).
Build with CMake and run CTest's `native-declaration-ingress` and
`native-ingress-differential`. These preserve the schema validators below and
accept syntax only, including syntactically valid but semantically invalid fixtures.

SN-020's [accepted declaration baseline](../../docs/experiments/SN-020-acceptance.md)
composes the preserved drafts with [project declarations](../../docs/architecture/PROJECT_SCHEMA.md).
All 94 tests run through discovery or CTest `schema-declarations`.
`python tests/schema/project.py tests/schema/fixtures/two-rc-project.json`
returns declarations only; every resource/runtime readiness flag stays false.
The [resource-links CLI](resource_links.py) also accepts
`fixtures/two-rc-resource-links.json`; `fixtures/invalid-resource-kind.json`
is expected to fail with `kind`. No backend or physical resource loader is tested.
Earlier counts below document the individual slices, not the current suite total.

The separate [resource lock draft](../../docs/architecture/RESOURCE_LOCK_DRAFT.md)
adds inert inventory metadata and lexical paths. All 64 tests run with the
discovery command below. Use
`python tests/schema/resource_lock.py tests/schema/fixtures/owned-resource-lock.json`;
`invalid-resource-path.json` must exit 1 with `path`. The
[report](../../docs/experiments/SN-020-resource-lock.md) distinguishes lexical
checks from pending physical containment and resource loading.

The [0.3 descriptor draft](../../docs/architecture/BINDINGS_DRAFT.md) adds separate
symbol/model interfaces and mappings. The complete schema tests run with
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
