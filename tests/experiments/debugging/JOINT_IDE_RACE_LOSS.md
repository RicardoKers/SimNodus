# Analog process loss during interrupt/breakpoint arbitration

Predeclare a real analog-worker termination after the delayed-host race reaches
its verified CPU endpoint at 4010750 ns. Require one retained real MI interrupt,
a read-only CPU observation before ADC after interception, and actual IDE
BREAKPOINT/PC/time confirmation. Before killing the worker, inspect it and
require equality with the last accepted joint state at 2011375 ns.

Kill only the owned analog process before advancing it or transferring ADC.
Require nonzero analog exit, failed acknowledgement, CPU cancellation accounting
with unused time and matching sinks, and no third checkpoint, ADC transfer or
verified SIGINT notification. Terminate Renode and close the relay before
publishing the session-failure diagnostic in the IDE. Keep the total IDE
request-to-diagnostic deadline at two seconds, including injected host latency.

Run three fault/recovery pairs in fresh sessions. Each recovery must start both
engines at zero, repeat the delayed arbitration and finish with ADC code 3541.
Require accepted IDE closure, zero IDE exit and no owned listeners. Run the
default guarded lifecycle regression after the shared Java/driver change.

Use --profile joint --guarded --joint-race --joint-wall-pace-ms 0
--joint-race-delay-ms 50 --joint-fault analog and the experimental Renode path.
Omit --joint-fault analog in a new output directory for recovery. This validates
abort and a new session; no rollback or resumption of the failed session is
supported. Other arbitration faults and the full E-05 gate remain open.

## Injection refinement

The first recovery completed the SIGNAL sequence and ADC result but failed the
collision-only criterion: a 50 ms delay alone did not always reach the ADC
breakpoint. Preserve race-analog-recovery-01 as a failed injection, not a
verified collision recovery. The delayed injector now waits at least 50 ms,
then uses read-only samples until CPU time reaches 4010750 ns before cancelling.
It still requires an earlier post-interception observation strictly before ADC
and retains the original two-second total deadline. No CPU wall pacing or
synthetic stop is introduced. Record actual delay and progress observations.

## Measured fault/recovery result (2026-09-09)

Three matching-source fault/recovery pairs (02-04) passed with the refined
injection. Each failure followed one real retained interrupt and an actual
IDE-confirmed CPU breakpoint at 4010750 ns. Before process loss, the analog
worker still matched the accepted 2011375 ns snapshot. It exited nonzero; no
ADC transfer, third joint checkpoint or verified SIGINT notification occurred.
Renode and relay teardown preceded the IDE diagnostic. Host handling took
236.5169, 388.6805 and 283.8699 ms, and the IDE enforced the two-second total
request deadline. All IDEs accepted closure and exited zero, with listener
cleanup. Each fresh recovery started at zero and completed the four joint
boundaries with one ADC transfer and firmware code 3541.

The successful first fault and failed first recovery are retained separately;
they predate the read-only breakpoint-wait refinement and are excluded from
the matching-source three-pair evidence.

The default guarded lifecycle regression retained the exact nine stop/circuit
records and reset/reconnect cleanup. [Consolidated evidence](../../../docs/experiments/evidence/E-05-race-analog-loss-summary.json)
records the three pairs and preserves the failed injection predecessor. This
closes analog process-loss abort and fresh recovery for this arbitration branch;
response timeout, CPU/debugger loss in the branch and the full E-05 gate remain
open. The paced/default transport ownership policy is unchanged.
