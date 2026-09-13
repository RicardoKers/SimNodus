# CPU backend loss before arbitration acknowledgement

Retain one actual MI interrupt, observe CPU time still before ADC, and use the
minimum-50-ms/read-only breakpoint injection. At 4010750 ns, ask the actual IDE
to confirm BREAKPOINT and PC 0x08000134. Do not cancel or complete the active
grant. Only after IDE confirmation, terminate the owned Renode process.

Require nonzero backend exit and relay detection of transport loss. The third
grant must remain armed and have no result file or acknowledged outcome. The
IDE observation is not a host cancellation acknowledgement. Inspect persistent
analog state after CPU loss and require exact equality with the last accepted
joint checkpoint at 2011375 ns. Terminate the analog worker and relay before
publishing failure, with no ADC transfer, third checkpoint or verified SIGINT.
Keep the two-second IDE request-to-diagnostic deadline, including the injection.

Run three fault/recovery pairs in fresh sessions. Each recovery must start at
zero, complete the delayed arbitration and store ADC code 3541. Require accepted
IDE closure, zero IDE exit and removed owned listeners. Regress the default
backend-loss profile and default guarded lifecycle after shared driver changes.

Use --profile joint --guarded --joint-race --joint-wall-pace-ms 0
--joint-race-delay-ms 50 --joint-fault backend and the experimental backend.
Omit the fault option in a fresh output directory for recovery. This covers
abort and new-session recovery at this injection point, not recovery of the
unacknowledged grant. Debugger loss in arbitration and full E-05 approval remain
open. Preserve all failures and matching source/binary hashes.

## Measured fault/recovery result (2026-09-09)

Three fresh matching-source CPU-loss/recovery pairs passed. After retained MI
interruption and actual IDE breakpoint confirmation, Renode exited nonzero
without completing the third host grant. The relay observed transport loss;
only the first two grants had acknowledged outcomes, and no third result file
existed. The persistent analog snapshot remained at 2011375 ns. No ADC transfer,
third joint checkpoint or verified SIGINT notification was published.

Host handling measured 183.0872, 149.8341 and 182.0160 ms, including the delayed
injection; each IDE independently enforced the two-second total request limit.
Every fresh recovery started at zero, completed the four arbitration boundaries
and stored ADC code 3541. IDE closure was accepted, IDE exits were zero and
owned listeners were removed. A pre-loss IDE observation did not become a
fabricated host grant acknowledgement.

The earlier non-arbitrated joint CPU-loss regression passed without a raw
interrupt, and the default guarded lifecycle retained exact reference stop/
circuit records and reset/reconnect cleanup. [Consolidated evidence](../../../docs/experiments/evidence/E-05-race-cpu-loss-summary.json)
records the pairs and both regressions. Debugger connection loss in arbitration
and the full E-05 gate remain open.
