# SN-021 managed-store visibility and path-object probe

## Prepared scope and acceptance

This manual, disposable-VM batch extends the bounded managed-store candidate on
local Windows/NTFS. It does not change the project declaration profile, native
resource verification, authenticated request protocol or runtime integration.
The existing valid project fixture is treated as inert bytes.

The batch is accepted only when its retained report and source/binary hashes show:

1. A committed revision 1 remains byte-identical before and after a writer killed
   just before rename. The independent namespace has no revision 2 at that barrier;
   an independent reader of the same `ManagedStore` reports `busy`, and another
   writer cannot acquire the lock. Reopening after the kill returns revision 1.
2. At the `published` barrier, the independent namespace contains revision 2 while
   the same-store concurrent reader reports `busy` and another writer is excluded.
   After exact-process termination, reopening returns revision 2. Independent
   bytes, record digest and file identity show a complete new record and unchanged
   old record. This is serialized-authority visibility, not a claim that a client
   can directly read the new file while its writer retains an exclusive handle.
3. Each case-only rename of the store root, document directory, manifest, lock and
   first committed leaf is independently injected on a fresh provisioned store.
   An ordinary writer refuses opening it; the injected name is confirmed with
   case-sensitive enumeration, and the original objects retain bytes, identities
   and security descriptors.
4. Each root/document junction and manifest/lock/record symbolic link is
   independently injected on a fresh provisioned store. The reparse attribute is
   observed on the link object itself; opening must refuse. Root, manifest, lock
   and record must reach the same-handle physical rejection. A document junction
   may instead be denied at the initial handle open with `STATUS_ACCESS_DENIED`;
   the fixture's inherited ACL is consistent with a missing `FILE_ADD_FILE`
   right. This is recorded separately and does not count as
   same-handle reparse-inspection evidence. The moved target retains bytes,
   identities and security descriptors. The probe never recursively enumerates a
   reparse point and never cleans up or retries a run.

Every failed or inconclusive attempt is retained. Exact success covers only this
fixture, VM filesystem and native candidate. It does not establish authenticated
Save, interface compatibility, origin trust, redistribution permission, arbitrary
external overwrite, power-loss durability, real disk-full behavior or simulation
readiness. A matching hash proves only these selected bytes, not authorization or
fitness for execution. The next gate remains authenticated managed requests and
composition with the previously accepted real RC lifecycle.

## Result

Three attempts are retained under `build/sn021-vm-store-visibility-runs/`. The
first, `20260925T223940Z-57fb140f433c`, passed the visibility group but stopped
before the first alias open because Windows PowerShell treated a case-only
directory `Rename-Item` as identical source and destination. The coordinator
then changed to no-replacement `MoveFileExW`; a Windows PowerShell 5.1 preflight
confirmed that case-only directory rename. The second,
`20260925T224901Z-79c0eeb64561`, passed visibility and all five alias cases,
then stopped at the document junction. Its initial writer open returned NT
`STATUS_ACCESS_DENIED` (`0xc0000022`) before physical inspection. The inherited
root ACL lacks the requested directory-add right, consistent with this result;
the junction's exact DACL was not independently captured. The acceptance
criterion now records this ACL denial separately and still requires physical
rejection for the other four reparse cases. Both failed reports and their source
snapshots remain unchanged.

The third attempt, `20260926T002659Z-a9e303e5475d`, completed in 5,146 ms on
Windows 11 Enterprise Evaluation build 26200 and local NTFS. Its guest report
SHA-256 is `58b78c51dd0fdca20eeb810766fb9b517a7da789324c845377e05670208084bc`.
The [published audit result](evidence/SN-021-store-visibility-result.json) pins
the source, binary, project and report hashes for all three attempts. The
[read-only auditor](../../tests/resources/audit_managed_store_visibility.py)
checked their retained reports and source snapshots without guest execution.
The complete run recorded 11 fresh roots and 54 observations:

- The same-store reader returned `busy` at the prepublication and published
  barriers. A second writer was excluded at both barriers. Before rename,
  independent namespace observation found no revision 2; after rename it found
  revision 2. Exact-process termination before publication reopened revision 1,
  while termination after publication reopened revision 2. Independent record
  digests matched their bodies, the old record retained its bytes and identity,
  and the new record had a distinct identity. The prepublication staging orphan
  remained in the namespace; recovery did not delete it.
- All five case-only aliases were refused. Root, document, manifest and lock
  returned `name-alias`; the committed leaf returned `unknown-entry`. The
  injected names were confirmed with case-sensitive enumeration, and the
  provisioned objects retained their bytes, identities and security descriptors.
- The root junction and three file symlinks returned `physical` after their
  reparse attributes were observed. The document junction returned
  `child-open`/`STATUS_ACCESS_DENIED` before same-handle inspection. All five
  moved targets retained their bytes, identities and security descriptors.

The Release probe compiled with MSVC; the PowerShell coordinator parsed; the
VIX host-only preflight passed; the complete Release build passed 53 CTest
entries. The repository checker and diff whitespace check passed. An initial
CTest invocation before the full Release build failed because most test
executables had not been built; the completed build and rerun resolved that setup
error. A host symlink preflight was inconclusive because the non-elevated host
token lacks symlink privilege; the successful elevated guest run supplied the
physical file-link observations.

This accepts only the bounded visibility/alias/reparse candidate on the tested
VM. The document-junction result is an access-control refusal, not independent
proof of its reparse inspection. The prior managed-store batch and selected-record
correspondence remain separate evidence. Authenticated Save requests, a
production reader endpoint, remaining request-boundary cases, real disk-full
behavior, power-loss durability and composition with the accepted RC lifecycle
remain outside this result. No source, model, firmware or dependency was executed
as a consequence of project opening. SN-021 remains in progress.
