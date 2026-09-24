# SN-021: bounded managed record codec

Date: 2026-09-24. Scope: [experimental canonical format](../architecture/MANAGED_RECORD_FORMAT.md)
under ADR 0077. No VM, storage mutation, backend execution or resource acquisition.

## Acceptance and observations

- Exact independently encoded canonical records decode and re-encode identically.
- Owned results survive mutation of the original encoded input.
- Invalid metadata, parent relations, SID/context shapes, lengths, digests and
  declaratively invalid projects reject, including adversarially recomputed hashes.
- Integer lengths are bounded before payload allocation; project, context, record,
  path and revision limits remain distinct. Lock hashing retains its 1 MiB budget.
- Missing resource locators remain inert metadata; successful decoding grants no
  readiness, compatibility, filesystem, authentication or execution claim.

Initial local MSVC 19.51.36246.0 / Python 3.14.4 run: **17 native contract checks and
370 independent cases passed**, plus the existing native lock contract regression.
Independent cases cover import, a subsequent same-content revision, revision 64,
1 MiB project, maximum SID/root, Unicode, truncated headers/payloads, concatenated
records, oversized/overflowing lengths, unknown formats/policies, null/inconsistent
parents, malformed SIDs, invalid Windows/UTF-8 roots and request/integrity tampering.
A synthetic accepted predecessor is not a validated physical chain or ABA test.

Final full local Debug build and suite: **52 CTests passed**, including schema,
resource, persistence, firmware/target and configured-replay regressions. No new
real-engine integration was needed: this byte codec changes no consumption path.

Commands (no dependency download):

```powershell
cmake -S . -B build/sn021-managed-codec -DBUILD_TESTING=ON
cmake --build build/sn021-managed-codec --config Debug
ctest --test-dir build/sn021-managed-codec -C Debug --output-on-failure
python tools/check_repository.py
```

The PR records the full local/hosted validation and source/squash identities.
The initial sandboxed CMake compiler detection was unavailable; approved execution
used the already installed MSVC. Initial test-target exception-option warnings were
resolved by enabling the same strict MSVC options for both new test executables.

## Limits and next step

No write/recovery/isolation result is claimed. A malicious actor can construct valid
hashes and identity declarations. The actual authority must authenticate the principal,
validate trusted generation/document/context and compare predecessor tokens against
retained physical observations. Fresh commit/operation IDs, duplicate receipts, stale
tokens, namespace membership and exclusive writer ownership remain store obligations.

Next implement the bounded chain/write candidate with exact provisioning/ACL checks
and injection barriers, then run the ADR 0077 write-enabled VM acceptance batch.
Preserve all 158 earlier JSON evidence files and twelve unrelated local SN-045 overlays.
SN-021 remains in_progress; this is not the final acceptance or next-cycle handoff.
