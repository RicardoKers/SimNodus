# Managed document saving contract

Date: 2026-09-24. Status: **required behavior under ADR 0076; unimplemented**.
This specifies acceptance, not an already validated algorithm or service API.

## Operations and authority

| Operation | Required behavior |
|---|---|
| Open external | Existing inert capture/validation; no implicit import or authority to overwrite |
| Import | Explicitly capture and validate owned bytes, create a fresh managed ID, record authorized resource-root bindings; leave external source unchanged |
| Open managed | Authenticate/authorize the caller, independently validate store state, return owned document bytes and an opaque expected-version token |
| Save managed | Validate exact bounded bytes; require caller authority and the current token; atomically update that managed document or preserve the prior committed version |
| Reconcile | Resolve an authorized operation ID after an uncertain outcome without replaying a blind overwrite |
| Export | Explicit create-only copy to a selected external destination; preserve existing entries and explicit resource-root meaning |

Authority comes from trusted provisioning and authenticated operations, never a
manifest path, document ID, digest, claimed SID or version token alone. Do not adopt
an existing user-writable directory or assume ACL changes revoke existing handles.
Only the authorized writer may mutate the fresh store; ordinary clients receive
owned bytes and bounded metadata, never writable filesystem handles.

Each writer launch validates relevant ancestors, owner/DACL rights, object types,
identities and bounded contents before exposing operations. Retain physical handles
where the accepted resource contracts require them. No textual-prefix check or
resolve-then-open establishes containment. The trust model includes the provisioner,
writer code/identity and OS; privileged administrator/kernel compromise is outside
ordinary-client exclusion. Do not mistake that exclusion for executable attestation.

## Bytes, resources and versions

Use the existing accepted project/document/resource limits; no limit is increased
by this contract. Validate document metadata before staging writes. Save persists
the exact validated owned document, preserving source positions and policy; it must
not fetch, execute, compile or render referenced resources. Missing resources can
remain an explicit readiness failure rather than a reason to mutate saved policy.

Managed metadata must explicitly bind the document to its authorized resource-root
context. Moving bytes into the store must not rebase relative paths. Persist that
context consistently with the committed revision or refuse ambiguity. Export does
not promise a self-contained package: preserve or explicitly rebind roots without
rewriting the document or copying dependencies silently. Metadata paths from the
project cannot grant access to an internal store object or change a trusted binding.

The expected token identifies the store generation, managed document and committed
revision/object identity. Its exact encoding belongs to the implementation design.
An A-to-B-to-A content sequence must have three distinct revisions; equal hashes
must not revive an old token. No counter wrap or restored store generation may
silently reuse a valid token. Import creates a new identity, not ownership of the
source pathname. Root-binding changes also invalidate the previous revision token.

## Publication and results

Serialize publication per document under the proven exclusive writer boundary.
Exactly one of two saves with the same expected token may commit. Validate the
current identity/version within that boundary; a stale request must not stage or
publish over a newer owner merely because its content matches.

Choose and document one authoritative atomic commit record binding revision,
document bytes and resource context before implementing writes. Independently
replacing a data file and version catalog is insufficient. The design must identify
the linearization point, protected staging ownership, read consistency and supported
filesystem/concurrency model. This contract selects no Windows replacement primitive.

| Result | Meaning |
|---|---|
| Committed | Publication is known to have happened; return the new token and reconcilable receipt |
| Rejected/not committed | Validation, authorization, stale version or definite prepublication failure; prior committed version is unchanged |
| Indeterminate | Publication may have happened; do not claim rollback or invite an unconditional retry |

Bind each operation ID to the authenticated principal, document, expected revision
and exact request contents. Reuse for a different request rejects. Reconciliation
must return the established outcome or explicit uncertainty, never guess from a
filename. A lost reply does not authorize another save. Specify bounded receipt
retention and capacity behavior; refuse safely rather than silently evicting an
operation still needed to resolve an uncertain outcome.

## Interruption and recovery

Test process interruption before/after staging, flushing, publication and response.
Recovery must expose the old or new complete committed revision, reject ambiguous
or corrupt state and preserve evidence. It must not delete an orphan by name alone.
Define bounds on scanning, bytes, records and cleanup; cleanup requires proven
ownership. Disk-full, denied access and write/flush failures require explicit results.

Atomic visibility and process-crash recovery do not prove power-loss durability.
State the tested durability boundary; do not promise stronger guarantees from an
ordinary successful flush or graceful restart. No installer, automatic service
startup, host provisioning or administrative repair follows from opening a project.

## Evidence sequence and stopping rules

1. Close the relevant remaining isolation prerequisites in the existing disposable
   VM: private-v2 identity/authorization/replay regression and ordinary-client direct
   mutation denial across writer termination/restart. Reuse unchanged evidence with
   explicit limits; do not expand the wire protocol or provision a service.
2. Specify the concrete commit layout, state machine, bounds and injection points.
   Review it against the properties above before implementing save/recovery.
3. Measure competing clients, stale/ABA requests, ordinary failures, interruption,
   lost replies, reconciliation and bounded recovery using real filesystem/process
   behavior. A mocked token/filesystem cannot prove isolation or atomic publication.
4. Compose import/open/edit/Save/reopen/export with the existing accepted RC profile,
   source mapping and resource bindings. Preserve external sources and runtime policy.
   Use real engine consumption where this integration changes the consumption path.
5. Perform the final bounded SN-021 audit; do not require new MCUs, runtime modes or
   UI features. Keep unknown/unverified readiness explicit.

The [current assessment](../experiments/SN-021-request-matrix.md) is input to step 1,
not evidence that these steps passed. ADR 0064 still prohibits unsupported external
overwrite. ADR 0076 changes the selected workflow, not the need for physical proof.
