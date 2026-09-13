# Joint IDE analog-loss candidate

Predeclare a real process-loss injection during the paced joint interrupt path.
After the actual IDE interrupt is intercepted and Renode cancellation is
acknowledged, kill the persistent analog helper before it can acknowledge the
third boundary. Attempt the same analog advance and require failure. Do not
send the verified GDB pause notification or publish the third joint checkpoint.
Terminate Renode and close the relay before announcing the failed session.

Require an actual Eclipse console-model diagnostic, no third inspected state,
only two accepted checkpoints, and acknowledgement within two seconds of the
interrupt. Require zero IDE exit, accepted workbench close, no owned listeners,
and nonzero analog-helper exit. Repeat three fresh failure/recovery pairs.
Each recovery must complete the joint sequence and firmware ADC code 3541.
Run the default guarded lifecycle regression after the shared driver/Java change.

The injection is only enabled by `--profile joint --guarded --joint-fault analog`
with the experimental backend. This tests one loss boundary, not the complete
joint timeout/reset/reconnect matrix or unpaced interrupt behavior. Existing
numerical tolerances, pacing, and startup/shutdown checks are unchanged.

## Measured result

Three fresh actual-IDE failure/recovery pairs passed. The host acknowledged loss
in 131.3522, 203.1762 and 139.4605 ms; the IDE independently enforced the two-second
interrupt-to-diagnostic deadline. Each analog helper exited nonzero, Renode
terminated normally, and only the first two checkpoints remained. No verified
GDB pause notification or third joint-state marker was emitted. The Eclipse
console model reported the failed session. All six IDE processes acknowledged
workbench closure and exited zero, with owned listeners removed.

Every fresh recovery restored the complete joint GPIO/RC/ADC sequence and
firmware code 3541. The default guarded lifecycle regression also passed:
all nine stops and circuit values exactly matched `resume-normal-04`, with
session recreation and reconnect at zero. Source/binary hashes matched across
all six joint runs. See [evidence](../../../docs/experiments/evidence/E-05-joint-ide-loss-summary.json).

Run with a new output directory:

```powershell
python tests/experiments/debugging/cubeide.py --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --output build/sn016/joint-loss-next --profile joint --guarded --joint-fault analog --experimental-renode build/sn016/source-notification
```

Run a fresh recovery by omitting `--joint-fault analog` and using another output
directory. Coverage remains limited to this paced analog-loss boundary; joint
timeout/reset/reconnect, unpaced behavior and the existing Eclipse log exception
remain open. SN-016 is not complete.

## Live-worker acknowledgement timeout (predeclared)

Add `--joint-fault timeout` to send the owned worker a `stall` injection after
CPU cancellation. The real ngspice helper remains alive, waiting on stdin and
withholding its reply. This represents a missing response, not a claim about
ngspice convergence failure. Require the existing two-second response deadline
to expire while the worker is alive; record elapsed wait, then terminate both
backends before publishing the IDE diagnostic. Allow at most five seconds from
IDE interrupt to failed-session acknowledgement, explicitly including the two-
second wait and teardown. The successful-pause and process-loss limits remain
two seconds. No numerical tolerance or virtual-time limit changes.

Require three fresh timeout/recovery pairs, unchanged final ADC code, no third
joint checkpoint or verified GDB notification on timeout, and automatic cleanup.
Also verify the earlier analog-loss case once and the default lifecycle once,
since the shared fault branch and native helper changed.

## Verified timeout result (2026-09-09)

Three live-worker timeout/recovery pairs passed. Each worker was still alive
when its two-second reply deadline expired. Host handling, including teardown,
completed in 2.1279229, 2.1482991 and 2.1537733 seconds, within the predeclared
five-second failure limit. The IDE independently checked that limit and displayed
the failed-session diagnostic. No third joint checkpoint or GDB pause notification
was published. All recoveries completed the persistent GPIO/RC/ADC sequence with
code 3541. All IDE sessions acknowledged closure and exited zero, with owned
listeners removed.

The earlier process-loss case also passed with the updated shared branch and
native helper. The default guarded lifecycle's nine stop/circuit records remained
exactly equal to `resume-normal-04`, including recreation and reconnect at zero.
[Evidence](../../../docs/experiments/evidence/E-05-joint-ide-timeout-summary.json)
contains matching hashes across the timeout/recovery runs and loss regression.
This verifies a live worker withholding a response, not solver nonconvergence.
Joint reset/reconnect and unpaced interruption remain open.
