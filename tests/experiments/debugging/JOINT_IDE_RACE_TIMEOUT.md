# Missing analog response during interrupt/breakpoint arbitration

After actual retained MI interruption and delayed-host breakpoint arbitration,
require CPU cancellation at 4010750 ns, matching sinks and actual IDE BREAKPOINT
confirmation. Inspect the persistent analog worker at the previously accepted
2011375 ns state. Inject its existing `stall` command: the real process remains
alive but deliberately emits no acknowledgement. This models a missing worker
response; it does not establish solver nonconvergence or CPU starvation.

Require the existing two-second analog acknowledgement deadline, the exact
timeout diagnostic, and proof the worker is still alive when the deadline
expires. Kill the owned stalled worker, terminate Renode and close the relay
before publishing the IDE session-failure diagnostic. Keep the pre-existing
five-second total timeout-case deadline, including injected host latency,
response waiting and teardown. Do not publish a third joint checkpoint,
transfer ADC voltage or fabricate a SIGINT notification.

Run three fault/recovery pairs. Each new session must start at zero and complete
the same arbitration with one ADC transfer and result code 3541. Require accepted
IDE closure, zero IDE exit and removed owned listeners. Regress the race analog
process-loss case and default guarded lifecycle after the driver change.

Use --profile joint --guarded --joint-race --joint-wall-pace-ms 0
--joint-race-delay-ms 50 --joint-fault timeout and the experimental backend path.
Omit the fault option and choose a fresh directory for recovery. The 50 ms delay
is a minimum followed by read-only observation of the breakpoint; its earlier
post-interception sample must still precede ADC. Recovery does not resume or
roll back the failed session. CPU/debugger loss in arbitration and full E-05
approval remain open.

## Measured fault/recovery result (2026-09-09)

Three fresh timeout/recovery pairs passed with matching source and binary
hashes. Each stalled worker remained alive through the missing-response deadline;
waits measured 2.0022431, 2.0057858 and 2.0068512 seconds. Total host handling,
including injected scheduling latency and teardown, took 2.2628851, 2.2406752
and 2.2737371 seconds, below the unchanged five-second total timeout-case limit.
The IDE independently enforced that total limit and published the timeout
diagnostic only after teardown.

No third checkpoint, ADC transfer or verified SIGINT notification was published.
Each new recovery started at zero, completed the four arbitration boundaries,
and produced ADC code 3541. IDE closure, zero IDE exits and listener cleanup
passed throughout. These results cover missing worker response, not solver
nonconvergence or continuation of a failed session.

The race analog process-loss regression passed, and the default guarded
lifecycle retained its exact nine stop/circuit records and reset/reconnect
cleanup. [Consolidated evidence](../../../docs/experiments/evidence/E-05-race-timeout-summary.json)
records the matching-hash pairs and both regressions. CPU/debugger loss in
arbitration and the full E-05 gate remain open.
