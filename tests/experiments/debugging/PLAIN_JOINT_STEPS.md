# Plain GDB persistent instruction and source steps

Measured 2026-09-09. This extends `joint_persistent.py` with optional `--steps`.
The existing four-stop mode remains the default. Step mode rejects fault injection
until that combination has an explicit integration contract.

```powershell
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --steps --output build/sn016/<fresh-directory>
```

Three fresh processes passed nine identical checkpoints: GPIO marker, one actual
MI instruction step, GPIO edge, source function entry, three actual MI next
commands, ADC sample and ADC commit. PC/time and source-line progression are
checked; step-over must remain in step_target. Each CPU grant is cancelled and
accounted before advancing the same persistent analog worker. Inspection holds
PC/mailbox, r0/sp/lr and both engine times stable for 100 ms. Circuit endpoint
accuracy remains 1 ps and trajectory accuracy 10 microvolts; final ADC is 3541.

Evidence: [summary](../../../docs/experiments/evidence/E-05-plain-joint-steps-summary.json).
Raw runs: `build/sn016/plain-joint-steps-01`; original four-stop regression:
`build/sn016/plain-joint-steps-control-01`, three fresh passes. Seven relevant
binaries match the actual IDE evidence, including both Renode assemblies, GDB,
firmware, control worker, analog worker and ngspice. Across 162 comparisons with
six IDE sequences, fixed times/GPIO agree and maximum analog difference is
3.5084e-11 V (the IDE additionally has a variable intermediate pause boundary).

This is fixed-stop reconciliation, not complete profile equivalence. Direct GDB
bypasses the guarded relay; asynchronous interrupt, arbitration
and combined fault handling remain outside this runner. Reset/reconnect is now
covered by the optional lifecycle extension below. E-05 remains incomplete.

## Recreated lifecycle extension

Add `--lifecycle` to the command above to execute two nine-checkpoint sequences
per fresh repetition. It requires `--steps` and excludes fault injection.
After the first sequence, both engines and GDB exit normally and old Renode
listeners must be gone before recreation. The second session starts with a zero
mailbox, zero virtual time and an untouched analog circuit. It executes actual
MI detach and target-select commands using the same GDB process. CPU and analog
state remain unchanged during 100 ms detached and reattached inspection holds.
The second complete sequence must equal the first, including registers, source
stops, cancellation accounting, analog samples and mailbox contents.

Three lifecycle repetitions passed (six sequences, 54 checkpoints) in
`build/sn016/plain-joint-lifecycle-01`. Three original four-stop regressions also
passed in `build/sn016/plain-joint-lifecycle-control-01`. All engine and GDB exits
were zero and listeners were removed. See
[lifecycle evidence](../../../docs/experiments/evidence/E-05-plain-joint-lifecycle-summary.json).
Reset here means complete session recreation, not in-place rollback or a raw
GDB monitor reset. Direct-GDB asynchronous pause integration remains open.
