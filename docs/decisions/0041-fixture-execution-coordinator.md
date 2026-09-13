# ADR 0041: Bounded Renode execution coordinator

Date: 2026-09-12. Status: accepted for the SN-017 extraction slice.

## Decision

Extract Renode execution state into `FixtureExecution` in application. It owns
readiness, cancellation and debugger reply readers, grant path, pending flags,
write counts and existing progress/pause deadlines. It borrows the CLI-owned
control channel. The CLI retains startup validation, joint-session ownership,
global pending checks, normalized acknowledgement mode and actual joint commit.

Start/readiness, cancellation/result ingress and debugger sample/notification
transitions share state and deadlines. Keep them in one coordinator rather than
exposing mutable reader state between objects. Composite grant/stop commands
reuse the same start/cancel handlers. Legacy begin/observe delegate to the
coordinator so reset and pending checks remain identical across both paths.

Preserve allowed grants, exact cancellation accounting, consumed/stale result
rejection, bounded sharing-lock retry, debug notification prerequisites and
original 1900 ms deadlines. The coordinator reports whether notification permits
commit; it does not commit. CPU acknowledgement remains distinct from joint
commit. New grants reset only the same readiness/debug fields as before.
Failed sessions are terminal. No cancellation or new host command is sent merely
because the coordinator is destroyed; channel lifetime remains with the CLI.

Unknown commands retain their arguments for other coordinators. Per-command
transport-result visibility and suppression after global rejection remain
unchanged. Debug JSON fields move alongside execution diagnostics; field names
and meanings are preserved. No adapter, engine capability or command allowlist
change is introduced. Core/domain gain no GDB, Qt or third-party types.

## Validation and next step

[Evidence](../experiments/evidence/SN-017-execution-coordinator-summary.json) and
[reproduction](../../tests/headless/EXECUTION_COORDINATOR.md) retain the real-engine
matrix and protocol regressions. Direct pipe tests verify borrowed lifetime,
independent instances and premature cancellation without a write or commit.
Existing cases verify readiness, grants, cancellation files, locked/late/stale
results, debug arbitration and notification deadlines.

Fixed boundaries and final start/cancel/notify counters match the previous cycle.
Asynchronous progress polling issued one or two samples instead of one in all
previous sessions; this host timing variation is recorded, not treated as a
fixed virtual-state invariant. One initial ingress test used the wrong input
format; the failed report is preserved and the corrected raw-grant test passed.

SN-017 remains in progress. Next extract the remaining command dispatch and
global pending/commit gates into a bounded application session, leaving CLI
startup and text I/O thin. Preserve protocol, endpoint ownership, deadlines and
separate commit. ADRs 0014/0027/0028 and all capability restrictions remain intact;
this does not validate arbitrary firmware, physical peripherals or unpaced runs.
