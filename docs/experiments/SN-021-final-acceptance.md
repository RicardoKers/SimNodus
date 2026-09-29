# SN-021 final bounded acceptance audit

Date: 2026-09-29. Status: **bounded SN-021 acceptance complete on measured
evidence; publication status is tracked by Git and the PR**.

## Decision and exact scope

SN-021 meets its selected acceptance gates for a Windows/local-NTFS native
project workflow with one managed document and the accepted fixed analog E-01
RC replay. The implementation consists of headless native libraries and
explicit experimental adapters/fixtures; this decision does not assert that a
production desktop application, installed service or general simulation kernel
exists. The original external project is acquired inertly, explicitly imported
into a controlled copy, opened by an authenticated client, name-edited, saved
under an expected version, reopened and exported by create-only publication.
Explicit fixed replay verifies bound resource bytes and interface policy,
compiles a netlist with source mapping, and only then hands it to real ngspice.
Open, import, Save and export never start an engine or execute a resource.

The original [acceptance matrix](SN-021-acceptance.md) recorded configured
correspondence, safe persistence and this final audit as the three remaining
gates. [ADR 0076](../decisions/0076-managed-document-saving.md) replaced
arbitrary external-directory overwrite with managed-copy Save while preserving
expected identity/version, atomic visibility and ordinary-failure safety.
External overwrite remains unsupported. This audit composes, rather than
relabels, the separately measured results below.

| Gate | Evidence and observed result | Acceptance limit |
|---|---|---|
| Declarative contract and independent graph | [SN-020](SN-020-acceptance.md), [native project](SN-021-project.md), [graph](SN-021-graph.md) and [connectivity compilation](SN-021-connectivity-compilation.md) retain source positions and stable identities | A valid declaration alone grants no resource access or execution authority |
| Local containment and byte capture | [Native resource snapshots](SN-021-native-resources.md), [acquisition](SN-021-acquisition.md) and [same-handle root binding](SN-021-root-binding.md) reject tested missing, alias/reparse, size/hash and replaced-root cases before compilation | Windows/local-NTFS handle-relative policy; no pathname lease or arbitrary filesystem support |
| Native edit, persistence and circuit lowering | [Revision](SN-021-revision.md), [create-only save](SN-021-save-create.md), [ideal RC compilation](SN-021-ideal-rc-compilation.md) and [fixed replay](SN-021-fixed-replay.md) preserve policy/source mapping and lower the selected R/C graph | One explicit fixed E-01 analog profile; unsupported schedules, modes and models remain refusals |
| Managed publication and recovery | [Store candidate](SN-021-managed-store-candidate.md) measured 64 revisions, ordinary-client exclusion, failure/interruption recovery, run fencing and receipt retention; [visibility/path-object batch](SN-021-store-visibility.md) measured serialized old/new visibility and alias/reparse refusals | Protected, exclusive-writer fixture on tested NTFS; no power-loss or real disk-exhaustion guarantee |
| Authenticated version and request boundary | [Sequential v3 batch](SN-021-managed-request.md) measured authorized/unauthorized requests, stale/A-B-A, operation collisions, older receipts and lost replies. The [v3 boundary batch](SN-021-v3-boundary.md) independently audited 34 sources, 60 artifacts, 19 processes and four records: one of two competing Saves committed; wrong-owner/rights and occupied endpoints refused; two uncertain terminal Saves reconciled; a silent client timed out without publication | Private test endpoint; no installed service or general network API |
| Composed configured lifecycle and real engine | [Managed RC lifecycle](SN-021-managed-lifecycle.md) independently audited exact import/Open/edit/Save/reopen/export, external-source/root preservation, negative resource controls and bound fixed E-01 compilation. Real ngspice consumed the authenticated reopened revision: 5,012 samples, 9.889724283951296e-08 V maximum analytical error below 10 microvolts, and final time within 1 ps of 5 ms | Manual explicit execution only; no general configured MCU, mixed-signal, debug or temporal runtime acceptance |

The [managed lifecycle summary](evidence/SN-021-managed-lifecycle-vm-success-summary.json)
pins the raw VM and ngspice result hashes. The
[v3 boundary summary](evidence/SN-021-v3-boundary-success-summary.json) pins
the raw host/guest reports, byte-audit result and four committed record hashes.
The [final evidence index](evidence/SN-021-final-acceptance-summary.json)
pins these and the relevant prior store, request, resource-binding, fixed-replay
and historical negative summaries by SHA-256.
The two earlier [v3 failures](evidence/SN-021-v3-boundary-attempt1-summary.json)
and [collection failure](evidence/SN-021-v3-boundary-attempt2-summary.json)
remain distinct negative evidence; the successful run did not overwrite them.
Earlier failed or inconclusive experiments and their hashes remain historical
records. No simulator input or engine code changed in the final request-boundary
block, so the accepted real-ngspice run is reused without claiming a new run.

## Separate decisions and explicit limits

Declarative validity, physical containment, verified bytes, interface
compatibility and execution authorization remain separate. A matching hash
establishes byte identity only; it does not establish trusted origin,
redistribution permission or fitness for a different engine. The fixed E-01
run was an explicit operation on audited, bound bytes. No project open, Save,
firmware reference, model path or manifest triggers downloads, DLL loading,
firmware boot, model execution or rendering. The existing explicit reference
target/real Renode evidence is separate and does not imply an integrated
configured MCU replay.

The v3 boundary batch measured a connected client that sent **no** message
through the five-second read deadline. It did not physically submit a short
complete v3 frame. The earlier v2 request matrix measured one-byte message
framing, and local v3 parser tests reject every truncated Open prefix. This
distinction is retained rather than claiming the v3 VM batch covered both.
In the managed-store visibility batch, the document-junction case was refused
by access control before same-handle reparse inspection; the other tested
links reached physical rejection. The test-only store supports one document
and at most 64 committed revisions. Synchronous filesystem calls are not
preempted by the pipe deadline. Neither ordinary flush nor process-crash
recovery proves power-loss durability. Real disk-full, production provisioning,
installation, service lifetime, arbitrary external overwrite, general
cross-platform paths and broader runtime profiles remain outside acceptance.

Existing project numerical limits, source positions and policy were unchanged.
SN-017 still owns Python preparation, GDB transport and fixture sequencing.
SN-044's UI/instrumentation direction, SN-045's independent headless direction,
MCU/toolchain independence, PDF auto-open suppression and the PID retry fix
remain untouched. This decision authorizes no implicit UI implementation or
other SN work. The next planned task is SN-022's bounded Qt module/worker
boundary selection under its own acceptance criteria.
