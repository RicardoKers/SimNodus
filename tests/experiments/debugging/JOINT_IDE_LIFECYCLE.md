# Persistent joint session recreation and reconnect

Predeclare three fresh IDE repetitions using `--profile joint --guarded
--joint-lifecycle` and the experimental backend. Each IDE must run the full
paced joint GPIO/RC/ADC sequence, then close the first Renode and analog worker
and remove the relay/backend listeners. Recreate both engines at zero and reuse
the same IDE process. Verify the new persistent circuit has zero time, voltage
and samples, and firmware mailbox is all zeros. Perform actual DSF detach and
reattach while no grant exists; require CPU and persistent analog state to stay
zero. Then execute the full joint sequence again, reaching ADC code 3541 and
4027000 ns. Preserve the existing time/voltage/inspection and startup/close gates.

This is reset by coordinated session recreation, not debugger-only machine reset
or in-place solver rollback. Existing policy still rejects unsupported reset
commands. Require identical known firmware boundary times before/after reset;
asynchronous interrupt timing and analog integration sample count may vary.
Each IDE exits normally, and all owned processes/listeners are cleaned up.
Run the default guarded lifecycle once as a regression of the shared driver and
Java orchestration. Unpaced interruption and untested fault boundaries remain
outside this result; do not mark the full E-05 profile complete from this test.

## Verified result (2026-09-09)

Three fresh IDE runs passed, each containing two complete joint sequences with
session recreation and actual DSF detach/reattach between them. Every new
persistent circuit had zero time, voltage and samples. Firmware mailbox and
CPU time were zero before disconnect and after reattach; the persistent circuit
also stayed zero during the disconnected interval. Both sequences reached the
known firmware boundaries and ADC code 3541. Previous engine/relay resources
were removed before the new session was announced. All IDE sessions accepted
workbench close, exited zero and removed their final owned listeners.

Source and binary hashes matched across the three repetitions. The default
guarded lifecycle regression retained exactly the nine baseline stop/circuit
records and reset/reconnect behavior. [Evidence](../../../docs/experiments/evidence/E-05-joint-ide-lifecycle-summary.json)
contains all six joint sequences and the intermediate zero-state observations.

Run with a new output directory:

```powershell
python tests/experiments/debugging/cubeide.py --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --output build/sn016/joint-lifecycle-next --profile joint --guarded --joint-lifecycle --experimental-renode build/sn016/source-notification
```

This closes the bounded session-recreation and reconnect-at-zero gate. It does
not support debugger-only reset, in-place rollback, or reconnection to an active
disconnected grant. Unpaced interrupt behavior, other fault boundaries and the
existing Eclipse log exception remain open; SN-016 remains in progress.
