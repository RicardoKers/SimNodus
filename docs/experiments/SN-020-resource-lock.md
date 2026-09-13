# SN-020 fourth slice: inert resource inventory

Date: 2026-09-13. Status: metadata slice passed; SN-020 remains **in_progress**.
The [contract](../architecture/RESOURCE_LOCK_DRAFT.md) and
[ADR 0048](../decisions/0048-declarative-resource-lock.md) specify a separate
resource lock 0.1. Existing topology versions remain unchanged.

The validator checks exact inventory version labels, provenance and license
declarations, notice references, sizes/hashes, inventory digest, portable lexical
paths, collisions and budgets. It opens no resources and performs no acquisition
or execution. Physical containment is specified, not implemented or tested.

All **64 tests passed**, including 18 new lock tests and the prior 46 schema
tests. Cases cover traversal/absolute/encoded paths, Windows devices, cross-package
case collisions, file/directory conflicts, hash tampering, byte/count bounds,
unknown execution fields, missing notices and malformed JSON. Tests preserve
round trips and show that self-consistent forged metadata does not verify a
resource. A no-I/O test forbids file opening and path resolution during parsing;
a separate test reads the two owned fixture files and confirms sizes and hashes.

The valid CLI exits 0 with one dependency, two resources and 10145 declared bytes.
All verification/readiness flags remain false. The invalid traversal fixture
exits 1 with `path`. Nineteen earlier 0.1/0.2/0.3 evidence hashes still match.
[Evidence](evidence/SN-020-resource-lock-summary.json) records Python/Windows
versions, commands, CLI outputs, source/fixture/contract hashes and limitations.
The repository checker passed (442 text files), and `git diff --check` passed.

```powershell
python -m unittest discover -s tests/schema -p 'test_*.py' -v
python tests/schema/resource_lock.py tests/schema/fixtures/owned-resource-lock.json
python tests/schema/resource_lock.py tests/schema/fixtures/invalid-resource-path.json
python tools/check_repository.py
git diff --check
```

The invalid CLI exit is expected. This is not a performance run: no virtual time
advances, and unit-test wall duration is not an engine baseline. No physical
symlink/junction/race or new engine tests were run. Hash syntax does not prove
origin, permission, containment or capability. Only existing owned declarations
and LICENSE are inventoried; there are no new third-party assets or binaries.

Next define explicit typed descriptor/resource references. Physical loading stays
in SN-021; board/MCU/firmware and temporal contracts remain pending. Earlier
engine evidence, tolerances, capabilities, PDF suppression, PID retry, Python
preparation/GDB/fixture ownership and ADRs 0027/0028 remain unchanged. This cycle
creates no further commit, push, issue update or release; the previous checkpoint
remains the recovery point for work preceding this slice.
