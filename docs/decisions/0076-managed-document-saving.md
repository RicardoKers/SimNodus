# ADR 0076: Select managed document saving for SN-021

Date: 2026-09-24. Status: **accepted product workflow and acceptance direction;
implementation and physical acceptance pending**.

## Decision

The owner accepts Save updating a copy managed by SimNodus. Explicit import leaves
the original external document unchanged. Save updates the managed document;
export uses explicit create-only publication and does not overwrite the source.
An ordinary open is not import and grants no saving or execution authority.

For SN-021, replace the requirement to implement overwrite in any selected external
directory with validated managed-document update under the
[managed saving contract](../architecture/MANAGED_DOCUMENT_SAVE.md). This supersedes
only the choice of saving destination/concurrency scope, not the safety properties
in [ADR 0064](0064-overwrite-ownership-boundary.md). Expected identity/version,
atomic visibility and preservation on ordinary prepublication failure still require
proof. External overwrite remains unsupported; [ADR 0063](0063-atomic-project-create.md)
continues to govern export and Save Copy.

## Boundaries

This accepts neither a production service nor a particular on-disk commit layout.
The distinct writer/controlled NTFS store is the measured candidate, not a completed
save implementation. Prior permission for disposable-VM evaluation remains in force;
no host account/ACL changes or service installation are authorized by this ADR.

Resource roots must retain explicit authority and meaning across import, Save and
export. Do not reinterpret relative resources under an internal storage path or
copy dependencies silently. Preserve the project schema, profiles, source mapping,
runtime/byte/interface distinctions and independent domain/adapters/instrumentation.

## Consequences and acceptance

The supported lifecycle becomes explicit import, managed open/edit/Save/reopen and
explicit export, in addition to existing create-only operations. Updating an external
editor's original file is not promised. Product installation and portable deployment
remain unresolved; no UI implementation is implied.

SN-021 remains in_progress until isolation, identity/version conflicts, atomic
publication, interruption/lost replies, bounded recovery and lifecycle composition
have appropriate evidence. Reuse unchanged engine evidence, but use real engines
when managed consumption changes integration inputs or behavior. An accepted
workflow or matching hash is not proof of readiness, trusted origin or redistribution.

Preserve historical failures and hashes, SN-017 Python/GDB/fixture control, SN-044,
MCU/toolchain independence, numerical bounds and PDF/PID fixes. The final next-cycle
prompt still requires actual bounded SN-021 acceptance.
