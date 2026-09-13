# ADR 0042: Bounded application session composition

Date: 2026-09-12. Status: accepted for the SN-017 extraction slice.

## Decision

Extract dispatch and global pending/commit gates into `FixtureSession` in
application. It owns the joint state and the execution, analog, ADC and inspection
coordinators. The CLI retains startup validation and endpoint ownership, constructs
one session, feeds text commands and writes diagnostics. Borrowed endpoints must
outlive the session. The session is noncopyable and exposes a read-only snapshot.

Keep command ordering: clear per-command analog visibility, apply all pending
gates, handle quit, then dispatch execution, ADC, inspection, analog and legacy
numeric operations. A rejected pending operation aborts before any handler can
advance state or write a command. Actual joint commit remains a separate command
requiring both execution notification and final-inspection prerequisites.
Normalized CPU acknowledgement remains unavailable with a native CPU channel.

The command result distinguishes a reply from successful/failed exit disposition.
A bare quit while stopped or failed requests exit zero; active state requests
exit one. Pending gates precede quit. The CLI emits no extra reply for exit
requests and retains exit one on uncoordinated EOF. Explicit abort retains all
coordinator cleanup actions. No automatic commit, new worker command or in-place
recovery is introduced. Diagnostics preserve the existing JSON protocol.

The four coordinators and adapters are unchanged. This is a fixture-specific,
text-protocol application session, not a general project loader or autonomous
engine runner. The external harness still owns GDB transport, process setup,
fixture progression and raw untagged-stream association. Core/domain remain free
of Qt and third-party types. Startup helper execution remains explicitly configured.

## Evidence and next step

See [evidence](../experiments/evidence/SN-017-application-session-summary.json) and
[reproduction](../../tests/headless/APPLICATION_SESSION.md). Direct session tests
cover independent state, separate commit, terminal failure, exit disposition and
pending-worker rejection of commit, quit, begin and normalized analog input.
Existing engine/process suites retain the actual integration evidence.

SN-017 remains in progress pending an acceptance audit. Next map its acceptance
criteria to the extracted native runner/contracts and real-engine evidence,
identify remaining Python orchestration explicitly, and decide whether the
bounded extraction can close or needs one concrete gap addressed. Do not infer
production readiness or expand capabilities from this structural extraction.
ADRs 0014/0027/0028, PDF suppression and all historical evidence remain intact.
