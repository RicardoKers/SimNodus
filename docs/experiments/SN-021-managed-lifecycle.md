# SN-021: bounded managed-document RC lifecycle

Date: 2026-09-29. Status: **bounded physical lifecycle and real E-01 replay passed; SN-021 remains in progress**.

This experiment composes the accepted managed-copy workflow in ADR 0076 with
the already accepted fixed E-01 replay profile. It is a disposable-VM, test-only
fixture, not a production service or UI. The exact original document and
resources are prepared in one external root. Explicit import captures a valid
project and the physical identity of that root from the same retained handle.
The trusted writer seeds one managed revision with those exact document bytes,
the original locator, and that observed identity. No padding, silent resource
copy, native library load or engine start is part of import.

An authenticated ordinary client must Open the managed document, edit only its
name through the accepted revision API, Save with the returned expected token
and a fresh operation ID, then Open again from a new request. The post-Save
bytes, context, token and persisted record must correspond exactly. Export is
an explicit create-only publication of the reopened document; a collision
preserves the existing destination. The external original must remain
byte-identical throughout.

Only a separate explicit fixed-replay request may compile the owned document
bytes returned by authenticated Open. It must compare the persisted root
identity with the same retained root handle used for physical resource capture.
Missing, replaced or hash-divergent resources may leave the inert managed Save
intact but must prevent compilation. Replacing the root with byte-identical
resources must also prevent bound compilation. Source-map offsets, complete
temporal policy, schedule, netlist, numerical tolerances and false readiness
flags must remain unchanged. After this succeeds, a protected test stage must
hand the exact resulting netlist to real ngspice under the existing 10
microvolt and 1 ps project limits. No project Open or Save may start ngspice.

Metadata validity, physical containment, byte verification, fixed-profile
interface compatibility and execution authorization are separate decisions.
Even a matching manifest/hash does not establish trusted origin, redistribution
permission or general simulation readiness. Existing request/store evidence
must not be relabeled as lifecycle evidence. Retain every failed or inconclusive
attempt, raw report and source/binary hash. Do not provision a service, change
host accounts or ACLs, restore a VM snapshot automatically, download assets,
overwrite an external document, or expand the accepted profile.

SN-021 remains in_progress after this experiment until the remaining applicable
competing-request, endpoint, identity and deadline gates and final bounded
acceptance audit are complete.

## Prepared test boundary and local preflight

The test-only importer seeds revision 1 from exact acquired document bytes and
the same-handle NTFS root identity. A separate ordinary client edits the name,
uses the authenticated v3 Open/Save request fixture, reopens revision 2, exports
to a new file and explicitly requests bound fixed replay. The disposable-VM
coordinator records process identity, wire frames, raw records, physical file
IDs, ACLs, outputs and hashes. It also tests export collision, a hash-divergent
schedule, a missing passive resource and a byte-identical replacement root.
Neither the compiler fixture nor its saved response files constitute a
production authentication endpoint; the independent auditor must correlate
those exact files with successful authenticated server decisions and committed
record bytes before any engine consumes the netlist.

The PowerShell coordinator parses, and the four local native preflight cases
pass with the Release acquisition and lifecycle probes. They use a synthetic
Open envelope and therefore establish parser, edit/export and physical
refusal behavior only. The exact package-generation dry run verified all four
resources and 55 distinct, present source snapshots. The VIX host preflight
connected without guest access. A separate exact-byte compiler preflight
checked both source-map elements and the unchanged fixed E-01 netlist digest.
The full MSVC Debug build and 56 CTests passed before the physical run. The
repository checker passed on 819 text files at that point. The local scripts
retain each attempt without automatic retry, cleanup or snapshot revert.

The first operator invocation from `C:\Windows\System32` failed during local
preparation, before opening the VM: a legacy schema test module read fixtures
relative to the process working directory. The raw failed report is retained.
The runner now anchors that import to the repository and restores the caller's
directory. A `--prepare-only` run from the same `C:\Windows\System32` directory
completed and verified 55 source snapshots without guest access. The
[local preparation summary](evidence/SN-021-managed-lifecycle-local-preparation-summary.json)
records both raw result hashes. This correction is not a physical lifecycle
result; the full VM command still requires a fresh operator invocation.

The first physical [VM attempt](evidence/SN-021-managed-lifecycle-vm-attempt1-summary.json)
reached exact import and authenticated Open/Save, then failed when the
coordinator read revision 2 before its writer process had exited. All 55 source
snapshots and 17 copied artifact hashes match the retained reports. The Save
reply is not promoted to independently audited persistence. The coordinator
now waits for its exact writer process to finish before direct record
correspondence, as in the earlier measured request matrix. No automatic retry,
process cleanup or snapshot revert occurred; a new fresh run is required.

The fresh [successful physical run](evidence/SN-021-managed-lifecycle-vm-success-summary.json)
used `SN021-authority-baseline` and completed in the disposable VM. The host
verified 55 source snapshots and 42 copied artifacts. The separate auditor
matched all artifact hashes, three authenticated Open/Save/Open decisions and
wire frames, exact original and name-edited project bytes, the two committed
records, root context and physical identity. It also verified create-only
export and collision preservation, changed-schedule and missing-resource
refusal, and refusal after byte-identical replacement of the resource root.
The raw host result, guest report, records and audit summary remain under the
retained run directory with hashes in the linked summary. Earlier failures
remain separate evidence.
The exact executed coordinator snapshot is retained. After the run, one empty
final line was removed from the workspace copy for `git diff --check`; both
source hashes and the byte-only difference are recorded in the summary.

Only after that byte audit, the separate real E-01 acceptance runner compiled
the authenticated reopened revision and matched the fixed netlist digest
`95471148afe803f2e190fbc2f282fd321708b1b41d986587900dbb45f97f4362`.
The pinned ngspice run exited successfully, its callbacks matched the vectors,
and 5,012 samples had maximum analytical error 0.099 microvolts, below the
project's 10 microvolt limit. The final time was within 1 ps of 5 ms.
These results establish this bounded manual lifecycle, not a general managed
runtime or automatic execution on Open/Save. The remaining applicable
competing-request, endpoint, identity and deadline gates require a final audit
before SN-021 can close.

For the disposable VM, build the Release probe targets, then run the local
`build/sn021-vm-managed-lifecycle-run.py` interactively with
`--existing-snapshot SN021-authority-baseline`. Credentials are hidden and
never written to the evidence directory. For another physical candidate run,
use `tests/resources/audit_managed_lifecycle.py` on its retained run
directory, then explicitly run
`tests/experiments/ngspice/managed_lifecycle_acceptance.py` with `--vm-run`
and a fresh `--output` directory. The latter audits before launching the
existing E-01 host, recompiles the exact managed project with source-map
checks, and accepts only the pinned fixed netlist and unchanged project
tolerances. The successful result above does not alone close SN-021.
