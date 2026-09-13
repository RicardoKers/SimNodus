# SN-020 typed descriptor/resource links

Date: 2026-09-13. Status: reference-validation block passed; SN-020 remains
**in_progress**. See the [contract](../architecture/RESOURCE_LINKS_DRAFT.md) and
[ADR 0050](../decisions/0050-typed-descriptor-resources.md).

The pure validator composes preserved topology 0.3 and resource lock 0.1, checks
explicit typed asset references, complete descriptor bindings and model entrypoint/
formal-token maps. Null never selects a fallback. Visual rebinding preserves the
graph, electrical models and parameters. No file is opened except the explicitly
supplied CLI metadata document; no renderer, model importer or backend is invoked.

All 79 schema tests passed (64 prior and 15 new). New cases cover missing IDs,
wrong kinds, duplicate roles, unused invalid assets, complete mappings, source-token
injection and case aliases, malformed input, budgets, round trips and no resource
I/O. Tests also show that a declared nonexistent entrypoint remains unverified;
source-interface verification is not falsely claimed. Separate trusted-fixture
tests verify the owned SVG/SPICE text and license bytes against their lock hashes.

The valid CLI reports five linked descriptors, two assets, three locked resources
and 1579 declared bytes, with all readiness/verification flags false. The invalid
kind fixture exits 1 with `kind` at `$/symbols`. This is expected rejection.

```powershell
python -m unittest discover -s tests/schema -p 'test_*.py'
python tests/schema/resource_links.py tests/schema/fixtures/two-rc-resource-links.json
python tests/schema/resource_links.py tests/schema/fixtures/invalid-resource-kind.json
python tools/check_repository.py
git diff --check
```

No new engine run or supported profile is established. The owned asset text is
metadata-test input, not a validated electrical or renderer implementation.
Physical containment, actual model/SVG interfaces and executable loading remain
SN-021 work. Project-level board/MCU/firmware and temporal/fidelity declarations
still need SN-020 contracts. SN-044 and its UI/measurement decisions are preserved.

The initial repository check found this report link missing while documentation
was being assembled. A later attempted project-schema write was rejected by
automatic approval review because of the account usage limit; it created no
project validator on that attempt. After the owner resumed work, the authorized
write succeeded normally. The subsequent project declarations bring the suite
to 94 tests; see the [final audit/evidence](SN-020-acceptance.md) for acceptance
and remaining runtime limits. No approval restriction was bypassed.
