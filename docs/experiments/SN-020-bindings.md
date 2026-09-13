# SN-020 third slice: separate symbol/model descriptors

Date: 2026-09-13. Status: slice passed; **SN-020 remains in progress**.
[ADR 0047](../decisions/0047-declarative-bindings-draft.md) records the
[experimental 0.3 contract](../architecture/BINDINGS_DRAFT.md).

The developer validator separates visual anchors, logical pins/ports and model
terminals. Symbol and model catalogs have explicit typed references. Complete
one-to-one pin/parameter maps, dimensional compatibility and full range
containment are checked. Null bindings are explicit absence, not fallback.
Every successful result reports `simulation_ready: false`.

All **46 tests passed**: 29 existing tests plus 17 binding tests. Coverage includes
round trips, symbol replacement preserving topology/model maps/resolved values,
name/order independence, missing/aliased mappings, parameter dimensions/ranges,
unused descriptors, budget limits and rejection of resource/execution fields.
An in-memory validation test forbids file opening. The original 0.1/0.2 source,
fixture, contract and ADR hashes still match their evidence.

The valid CLI fixture reports three symbol bindings, two model-interface bindings
and 36 additional descriptor/binding entries. The invalid model-alias fixture
exits 1 with `mapping`, as expected. See
[evidence](evidence/SN-020-bindings-summary.json).
The repository checker passed (434 text files), and `git diff --check` passed.

```powershell
python -m unittest discover -s tests/schema -p 'test_*.py' -v
python tests/schema/bindings.py tests/schema/fixtures/two-rc-bindings.json
python tests/schema/bindings.py tests/schema/fixtures/invalid-model-alias.json
python tools/check_repository.py
git diff --check
```

No graphic, electrical implementation, model resource, native loader or backend
was executed. Fixtures contain owned interface declarations, not vendor assets.
Hidden pins, multi-unit/partial mappings and per-instance model selection remain
outside this draft. Next specify locked resource/dependency metadata and path
containment, with origin/license records and no automatic download/execution.
Board/firmware and temporal configuration remain pending; SN-021 stays planned.

The documentation write was initially rejected by automatic approval review
because usage was exhausted. After the owner resumed work, normal workspace
checks and the same authorized documentation work succeeded. Code/test results
were preserved. The owner separately authorized a source checkpoint commit and
GitHub push on 2026-09-13; no release or issue synchronization was requested.
