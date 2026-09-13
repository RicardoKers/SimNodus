# Debugger loss while an arbitration grant remains pending

Retain one actual MI interrupt, observe CPU time before ADC, then use the
minimum-delay/read-only breakpoint injection. At 4010750 ns, require actual IDE
BREAKPOINT and PC confirmation while the host grant is still armed. The IDE
runner identifies exactly one own descendant matching its configured GDB path
and forcibly terminates only that process. Do not issue orderly detach.

Require relay EOF/socket-loss detection with no rejected command payload,
Renode still alive, a grant armed and interrupt pending. Retain the upstream
connection until the host cancels the CPU grant, validates unused time and sink
agreement, and checks persistent analog state remains at the accepted 2011375 ns
checkpoint. Terminate both engines and close the relay before publishing failure.
No third joint checkpoint, ADC transfer or verified SIGINT notification may be
published. Keep the two-second IDE request-to-diagnostic deadline including the
injection and teardown. A CPU cancellation is not a joint confirmation.

Run three fault/recovery pairs in fresh sessions. Every recovery starts at zero,
completes arbitration and stores ADC code 3541. Require accepted IDE closure,
zero IDE exit and no owned listeners. Regress the earlier non-arbitrated joint
debugger-loss profile and the exact default guarded lifecycle after the shared
Java/driver change. Preserve hashes and all failed candidates.

Use --profile joint --guarded --joint-race --joint-wall-pace-ms 0
--joint-race-delay-ms 50 --joint-fault disconnect and the experimental backend.
Omit the fault in a new output directory for recovery. This is abort and new
session recovery, not resumption of a disconnected active session. Full E-05
approval and general workload coverage remain separate from this bounded test.

## Measured result (2026-09-09)

Three fresh debugger-loss/recovery pairs passed with matching sources/binaries.
Each loss occurred with Renode alive, a host grant armed and an actual retained
interrupt pending, after IDE breakpoint confirmation. The host cancelled the
grant at 4010750 ns with unused time and matching sinks, then verified the
analog state still equalled the accepted 2011375 ns checkpoint. CPU cancellation
was recorded, but no third joint checkpoint, ADC transfer or SIGINT notification
was published. Host handling took 308.3717, 291.6004 and 339.9856 ms; the IDE
also enforced its two-second request-to-diagnostic deadline.

Every fresh recovery started at zero and completed the four joint boundaries
with ADC code 3541. All IDEs accepted closure and exited zero, with listener
cleanup. The prior non-arbitrated disconnect profile passed with no MI interrupt;
the default lifecycle retained exact stop/circuit records and reset/reconnect
cleanup. [Consolidated evidence](../../../docs/experiments/evidence/E-05-race-disconnect-summary.json)
records all runs. These are bounded abort/recovery results, not continuation of
a disconnected session or complete E-05 approval.
