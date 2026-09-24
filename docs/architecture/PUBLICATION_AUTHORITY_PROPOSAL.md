# SN-021 publication authority proposal

## Consolidated measured coverage (2026-09-24)

The [request matrix assessment](../experiments/SN-021-request-matrix.md) maps the
filesystem, startup, identity, endpoint/restart and private-v2 response evidence to
the six gates below. It supersedes historical statements that no physical work has
occurred, without declaring all gates passed. Lifetime under interruption, complete
v2 regression and production ownership/recovery remain unproven. Review only
acceptance-critical gaps next; no service, managed-store adoption or change to the
arbitrary-directory overwrite criterion is selected by these observations.

## Next experiment contract

The [authority request contract](AUTHORITY_REQUEST_EXPERIMENT.md) specifies the
next read-only IPC/startup probe and its rejection matrix. It is unimplemented and
does not adopt this proposal or change ADR 0064. The filesystem observations below
remain the only new physical evidence; a pipe name or claimed SID is not authority.

## Subsequent physical evidence (2026-09-23)

The owner subsequently supplied a disposable VMware VM and authorized the bounded
account/ACL fixture experiment, without installing a service. The [report](../experiments/SN-021-authority-filesystem.md)
records 18 denied client operations around authorized writer replacement. This
updates the historical unmeasured state below, but does not pass all six gates or
adopt managed storage. IPC, broker startup/restart, token/path authorization,
expected-version/ABA and recovery remain unmeasured. The host was not reconfigured.

Date: 2026-09-23. Status: **proposal for review; not selected or implemented**.
The owner selected feasibility evaluation on 2026-09-23, explicitly without service
installation. This selects research direction only, not deployment or architecture.
This document does not supersede [ADR 0064](../decisions/0064-overwrite-ownership-boundary.md).
SN-021 remains in progress. TxF is excluded by the owner's explicit decision.

## Problem and alternatives

The existing save API creates a new document in an explicitly selected directory.
It does not own that directory's other writers. The [rename counterexamples](../experiments/SN-021-overwrite-boundary.md)
and [oplock observations](../experiments/SN-021-oplock-boundary.md) did not establish
continuous exclusion across replacement. This is not a general impossibility theorem.

| Candidate | Authority available | Assessment |
|---|---|---|
| Arbitrary selected directory plus lock file | Cooperation from participating writers | Insufficient under ADR 0064; unrelated writers retain access |
| Application folder and helper under the desktop user's identity | Same user's filesystem rights | A folder name or separate process does not isolate equivalent access tokens |
| Fresh store with a distinct restricted writer identity | Potential OS-enforced exclusion plus serialized requests | Concrete research candidate; changes installation and save workflow; unvalidated |
| Kernel component sharing oplock keys | Potential different protocol | Not investigated; driver deployment/maintenance exceeds this proposed next slice |

Windows checks file access against the caller's token and the object's security
descriptor. Child-object permissions must be considered individually; protecting
only a parent pathname is insufficient. Moving a file does not automatically give
it a fresh default descriptor. These facts inform the candidate, not a proof of it.
[Microsoft file security](https://learn.microsoft.com/en-us/windows/win32/fileio/file-security-and-access-rights).

## Proposed bounded candidate

Use a dedicated local writer with a distinct service identity and a newly
provisioned NTFS store. The desktop application remains an ordinary client. A
service-specific SID is a candidate mechanism, not a selected account or deployed
service. Microsoft describes adding a service SID to the process token; installing
a service requires the appropriate SCM authority. Evaluate a least-privilege
configuration rather than defaulting to LocalSystem.
[Service SID](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_sid_info),
[SCM rights](https://learn.microsoft.com/en-us/windows/win32/services/service-security-and-access-rights).

The proposed trust model includes the OS, installer, broker binary/configuration
and administrative maintenance. It excludes direct filesystem writes by ordinary
clients, including another process under the interactive user's identity. Kernel,
administrator and backup/restore authority can bypass this boundary and must not
be portrayed as excluded adversaries. Compromise of the authorized broker is also
outside this isolation claim. Merely launching a helper under the same token fails
this model. Avoid adopting any existing user-writable directory or changing its ACL.

Provision the fresh root and every data/temporary/metadata object with verified
ownership and permissions from creation. Deny clients effective write, replacement,
link, ownership and permission-changing authority, considering ancestor deletion
and inherited grants. Do not expose writable handles or accept caller-supplied
paths into the store. No preexisting external write handles may be assumed revoked
by later ACL edits. Handle-relative containment, reparse/alias rejection, physical
identity checks and byte limits remain required inside the controlled namespace.

Authenticate local IPC clients; associate opaque document IDs with authorized users.
A document ID, expected-version token or request ID is not itself permission. Define
request size/time bounds, disconnect behavior and impersonation boundaries before
implementation. Treat even authenticated project bytes as untrusted. Opening a
project must never install/start the service, provision storage or grant authority.

## Proposed operation and linearization

1. Explicit import captures and validates a bounded project document. Create a new
   managed ID; never claim ownership of or overwrite the imported source file.
2. Read returns owned bytes and a broker-issued version token bound to managed ID,
   store generation, current physical identity and byte version. None comes from
   project metadata. Revisions must distinguish an A-to-B-to-A sequence.
3. Save accepts validated exact bytes and the expected token. A single authorized
   publication owner serializes commits per document, rechecks identity/version,
   and rejects stale requests before publication. Multiple clients may prepare
   concurrently; exactly one save from the same expected version may commit.
4. Stage bounded bytes in an exclusively created object, verify its permissions,
   flush as specified, and atomically publish under the proven exclusion boundary.
   Select the actual on-disk layout and primitive only after the isolation proof.
   Do not independently replace a data file and version catalog and call both
   atomic. One authoritative commit record must bind version and immutable bytes.
5. Return a committed receipt, a definite prepublication failure, or an explicitly
   indeterminate result. A lost response must not invite blind overwrite retry.
   Query/retry by authenticated request ID must reconcile the durable commit state.

These steps are a protocol sketch, not a tested algorithm. The publication event
must be identified and measured before a success guarantee. Crash recovery,
orphan ownership, operation-ID retention and cleanup bounds need a concrete layout.
Fail closed on ambiguous recovery; never delete an orphan merely by filename.
Ordinary prepublication failure preserves the previous committed version. Atomic
visibility and power-loss durability are separate claims; neither is newly accepted.

## Product consequences and scope decision

Managed save would update a managed document, not the arbitrary imported pathname.
Export remains the existing explicit create-only save/copy. External editors would
require export and a new import/version workflow. A portable JSON document can remain
unchanged, but broker bookkeeping is a new storage contract. Resource roots must
remain explicit: moving the document must not silently reinterpret resource paths,
copy dependencies, widen accepted profiles or authorize execution. Domain and
instrumentation stay independent of Windows IPC and presentation.

A service requires installation, upgrade/removal, per-user authorization, recovery
and deployment policy, including classroom machines without administrator access.
Linux needs a separate authority implementation later; it is not enabled here.
This is a material architectural/workflow change, not a hidden implementation detail
of save_new_project. It would require a new ADR and explicit acceptance-scope decision.
It must not be used to declare arbitrary-directory overwrite solved.

## First experiment if selected

Recommend evaluating only authority feasibility first, in an explicitly authorized
isolated Windows test environment with two genuinely distinct principals. No engine,
UI, project-format change or production service is needed for that experiment.

| Gate | Required evidence |
|---|---|
| Provisioning | Fresh root and every object have measured identity/permissions; unexpected grants reject |
| Exclusion | Independent ordinary-client processes fail direct write, replace, rename, link and ACL/owner changes |
| Lifetime | Exclusion survives writer-handle close, broker stop and restart; startup validates the store |
| Authorized access | Dedicated writer can create/publish; a second unauthorized process cannot masquerade as it |
| Client boundary | Unauthorized ID/token/path requests reject; no writable handle escapes |
| Preservation | All original external fixtures and historical evidence remain unchanged |

Do not substitute an AccessCheck-only or mocked-principal test for real independent
processes. This first experiment would not yet accept save/recovery. If it passes,
then specify and test the single commit record, stale/ABA conflicts, simultaneous
clients, interruption at every publication boundary, lost replies, disk failures
and bounded recovery. Reuse engine evidence unless engine inputs/behavior change.

## Current recommendation

The distinct-authority candidate warrants a bounded feasibility evaluation only
if managed storage and an installation boundary are acceptable product directions.
Do not implement or install it based on this draft. If arbitrary-directory overwrite
must remain the only acceptable workflow, keep researching that unchanged contract
with a time-bounded candidate and falsifiable claim; do not repeat rejected probes.
No automatic closure, TxF fallback or reduction of SN-021 scope is proposed.

## Authorized feasibility phase

The owner selected evaluation of managed storage without installing a service.
First specify an isolated Windows environment with a dedicated writer principal,
an ordinary interactive client and a second client process with equivalent ordinary
rights. Do not create accounts, change ACLs or install/provision a service as a side
effect of this document. Identify an existing suitable disposable environment or
prepare a separately reviewable provisioning plan. No such environment was used here.

The distinct-writer trust model is a plausible design inference from Windows access
control, not demonstrated feasibility. None of the six physical gates has passed.
A same-user helper, restricted-client-only exercise or AccessCheck model would not
prove exclusion of another unrestricted ordinary client. The current host has not
been reconfigured. Final workflow adoption, any replacement of the original overwrite
acceptance criterion and installation authorization remain separate decisions.

## Reviewable environment plan, not provisioning instructions

Use an existing disposable Windows VM with local NTFS and a recoverable snapshot;
exclude repository worktrees, personal folders and live projects. The proposed first
experiment needs no installed broker service: an explicit test writer can run under
a dedicated ordinary local account, with test clients under another ordinary account.
That would test distinct-principal filesystem exclusion, not service-specific token
configuration, deployment or production IPC. Those would remain later gates.

Record OS/build, filesystem, effective token groups/privileges, owner and DACL on every
fixture object. An administrator would provision only a fresh test root and test
identities, then leave the measured writer/client operations unelevated. Confirm
neither client inherits writer/admin groups or writable handles. Exercise both direct
file writes and namespace/security changes from independent client processes, including
between writer sessions. Preserve reports and the snapshot; no automatic cleanup of
unidentified objects. Produce only fixture bytes, never actual projects or credentials.

A read-only discovery on 2026-09-23 found no Get-VM, VBoxManage or vmrun command in the
current shell. This is not proof that no VM exists or that virtualization is unavailable.
No VM, account, ACL or service was created. The raw discovery is retained under ignored
build/sn021-authority-environment-discovery.json (SHA-256: `0a38cee8b8d4c79a269349f32d8a8a5224bcb6261e5eb79404df2c8a4e4d6d37`).
Before physical execution, identify an existing disposable Windows environment and
its authorized setup boundary. Do not expand to host account changes, installation,
downloads or synthetic substitutes just because such an environment is not identified.
