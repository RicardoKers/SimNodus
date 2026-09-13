# CPU backend loss during an active joint grant

Inject real Renode process termination only after the actual IDE has resumed
and the host has observed CPU progress beyond GPIO high in the paced interval.
Require an armed, healthy grant and no MI interrupt. The IDE requests the fault
through its owned runner; the host terminates its own Renode process.

Require a nonzero backend exit and relay detection of transport loss. The active
grant must have no completion file or cancellation acknowledgement. Do not
complete its accounting or infer a final CPU time. Inspect persistent analog
state and require exact equality with the last accepted checkpoint (2011375 ns).
Terminate the analog worker and close the relay before publishing the failure
diagnostic. Require no third checkpoint, joint-stopped marker, or verified GDB
pause notification. The actual IDE must acknowledge the diagnostic within two
seconds of its request and close successfully; owned listeners must be removed.

Run three failure/recovery pairs in fresh directories. Each recovery must start
both engines at zero and complete the joint sequence with ADC code 3541. Compare
the default guarded lifecycle against the existing nine-stop/circuit reference
after the shared Java/driver changes. Preserve source and binary hashes.

```powershell
python tests/experiments/debugging/cubeide.py --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --output build/sn016/joint-backend-loss-next --profile joint --guarded --joint-fault backend --experimental-renode build/sn016/source-notification
```

Omit the fault option and use a fresh directory for recovery. This verifies
abort and fresh-session recovery, not resumption or rollback of the lost grant.
The paced interval restriction and the remaining E-05 gates still apply.

## Verified result (2026-09-09)

Three actual-IDE failure/recovery pairs passed with matching source and binary
hashes. Renode exited nonzero after the active grant began. The relay detected
transport loss; only the first two grants had acknowledged outcomes. The analog
state remained exactly at 2011375 ns, and no third checkpoint or verified pause
notification was published. Host handling took 70.1421, 69.6060 and 73.2565 ms;
the IDE also enforced its two-second request-to-diagnostic deadline.

All fresh recoveries started at zero and completed five stable checkpoints with
ADC code 3541. Every IDE accepted closure, exited zero and removed its owned
listeners. The default lifecycle regression retained the exact nine stop/circuit
records and reset/reconnect cleanup. See the [consolidated evidence](../../../docs/experiments/evidence/E-05-joint-ide-backend-loss-summary.json).
SN-016 remains in progress; unpaced interruption, other fault boundaries and the
existing Eclipse log exception are still open.
