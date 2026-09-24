# ADR 0064: Preserve the overwrite ownership boundary

## Subsequent workflow decision (2026-09-24)

[ADR 0076](0076-managed-document-saving.md) selects managed-document updates instead
of arbitrary external-directory overwrite for SN-021 acceptance. The ownership,
identity/version, atomicity and preservation requirements below still apply under
the new explicitly controlled concurrency model. External overwrite remains
unsupported; no historical counterexample or failed result is superseded.

Date: 2026-09-15. Status: accepted boundary decision; overwrite remains unsupported.

## Decision

Retain [create-only publication](0063-atomic-project-create.md). Do not add an
overwrite option to it or interpret a successful byte/hash check as destination
ownership. The [physical counterexamples](../experiments/SN-021-overwrite-boundary.md)
reject the candidate Windows/NTFS protocols tested in this slice.

A retained handle with no delete sharing protects the current file but prevents
both tested rename variants and ReplaceFile from replacing it. Releasing that
handle admits a concurrent owner. Delete sharing permits POSIX replacement while
the retained handle continues to read the original file; it does not bind that
file to the destination name. Rechecking identity before replacement still leaves
a gap. Moving the original to a backup first exposes an absent destination.

Accept future overwrite only with a demonstrated protocol that binds replacement
to the expected destination identity and version, preserves existing data on
ordinary failure, and provides atomic publication under its explicitly supported
concurrency model. Ownership must come from a trusted operation outside project
metadata. An advisory lock used only by cooperating SimNodus writers does not
exclude other writers in the current selected directory contract.

## Scope and consequences

This is a rejection of particular protocols, not a proof that safe overwrite is
impossible on Windows. No directory ACL changes, privileged helper, transaction
system, oplock protocol, journal or recovery mechanism is selected or validated.
There is no production overwrite implementation hidden behind the test program.
The test uses only fixture-owned files; all production path/handle rules remain
unchanged. Namespace atomicity, crash durability and execution approval stay
separate. Historical evidence and the create-only API are preserved.

SN-021 remains in progress. Overwrite is pending with a concrete acceptance gate;
repeating check-then-replace variants is not the next step. Proceed with bounded
native acquisition of a selected project document: retain physical handles while
capturing at most the accepted byte limit, validate/load that owned capture, and
return no pathname lease, save authority or resource/runtime approval. This is
a prerequisite for a future explicit edit/version contract and independent of
solving overwrite. Compilation still needs its own real-engine acceptance.
