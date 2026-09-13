# Extended IDE steps with fault and arbitration integration

Predeclared 2026-09-09 for SN-016. Combine the seven accepted instruction/source
step checkpoints through 2012875 ns with the existing asynchronous fault and
minimum-50-ms delayed arbitration profiles. Preserve all fixed times, PCs,
STEP/source-frame evidence, stable registers, 1 ps clock agreement and 10
microvolt analytical tolerance. Only the asynchronous paced profile sleeps in
time callbacks; delayed arbitration runs without CPU wall pacing.

Require real MI interruption retained once at the relay for arbitration.
Observe CPU time strictly after the last step and before ADC after interception,
then wait at least 50 ms and observe the ADC breakpoint at 4010750 ns. Require
actual IDE BREAKPOINT confirmation. Successful arbitration has nine joint
checkpoints, one ADC transfer, no synthetic SIGINT, no duplicate ADC grant,
and firmware code 3541 after fresh authorization.

For each of analog process loss, live analog response timeout, CPU backend
loss and debugger process loss, run three fault/fresh-recovery pairs with
extended steps and delayed arbitration. Failure must preserve exactly seven
joint checkpoints and the last persistent analog state at 2012875 ns. No ADC
transfer, eighth joint acknowledgement or verified SIGINT is allowed. CPU loss
has seven acknowledged grants and no eighth cancellation result. Debugger loss
has eight CPU acknowledgements, but still only seven joint checkpoints. Analog
loss and timeout must record the verified cancelled CPU grant while rejecting
the analog commit. Preserve two-second diagnosis limits except the existing
five-second total live-response-timeout limit (including its two-second wait).

| Arbitration outcome | CPU grant acknowledgements | Joint checkpoints | ADC transfers |
| --- | ---: | ---: | ---: |
| Analog process loss | 8 | 7 | 0 |
| Live analog response timeout | 8 | 7 | 0 |
| CPU backend loss before cancellation | 7 | 7 | 0 |
| Debugger loss followed by CPU cancellation | 8 | 7 | 0 |
| Successful fresh recovery | 9 | 9 | 1 |

Each fresh recovery starts at zero and completes the same extended arbitration
with ADC 3541. Require automatic accepted IDE closure, zero IDE exit and removed
owned listeners. Preserve all failed candidates and source/binary hashes.
Regress each extended paced fault once with fresh paced recovery, the extended
paced lifecycle, the shorter delayed arbitration, and default guarded lifecycle.
These bounded checks do not establish general unpaced timing, solver
nonconvergence recovery, rollback, or a production adapter capability.

## Host observation scheduling refinement

The isolated `steps-race-analog-01` fault passed. The first fault in each of
`steps-fault-matrix-01` and `steps-fault-matrix-02` failed the injection
precondition: the post-interception sample already equalled 4010750 ns. Both
runs retained seven checkpoints and published no eighth joint acknowledgement.
Preserve these failures. They do not establish an analog fault/recovery pair.

For subsequent candidates, poll the host coordinator every 1 ms only while
the asynchronous arbitration index is active, instead of 10 ms. Record this
host scheduling setting. Keep the strict post-interception pre-ADC observation,
minimum 50 ms injection, CPU without wall pacing and all numerical/deadline
criteria. This is test-host scheduling, not guaranteed unpaced product behavior.

The 1 ms polling candidate (`steps-fault-matrix-03`) passed its first analog
fault but its recovery failed the same pre-ADC observation criterion. Keep it
failed. Polling alone did not make the required injection reproducible.

The next explicitly separate candidate adds `--joint-race-initial-pace`: apply
1 ms callback pacing until a real interrupt is retained and the existing
read-only sample proves CPU time strictly between the last step and ADC. Then
release callback pacing through a test-only monitor command, requiring its
acknowledgement, before waiting for the breakpoint and cancelling. The minimum
50 ms delay and total deadlines still include this handshake. The release
changes only wall sleep; it advances no virtual time, halts no CPU and fabricates
no debugger notification. Record this as **initially paced arbitration**, not
fully unpaced evidence. The matrix runner uses this explicit setting for its
arbitration cases, including the shorter arbitration control. Earlier unpaced
results remain historical evidence with their own source hashes.

## Transient result-file access (2026-09-10)

`steps-fault-matrix-04` passed 23 cases before the last debugger-loss recovery
failed with PermissionError reading `joint-7.txt`. The file subsequently became
readable and contained valid cancellation at 4010750 ns. Preserve that failed
recovery and its seven prior checkpoints; no joint pause was published.
The result reader now treats temporary PermissionError as not-yet-readable and
retries on subsequent polls under the existing phase deadline. It never fabricates
an outcome, increases the timeout or changes file permissions. A focused test
requires no acknowledgement on denied access and failure at the original deadline.
Repeat the matrix on this final source set before reporting integrated success.

```powershell
python tests/experiments/debugging/cubeide.py --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --output build/sn016/<fresh-directory> --profile joint --guarded --experimental-renode build/sn016/source-notification --joint-steps --joint-race --joint-wall-pace-ms 0 --joint-race-delay-ms 50 --joint-race-initial-pace --joint-fault analog
```

Use `timeout`, `backend` and `disconnect` for the other faults; omit the fault
option in a fresh directory for recovery. For paced coverage omit the three
race/pacing arguments and initial-pacing flag. Full E-05 gate review remains a
subsequent decision. `steps_fault_matrix.py` runs all 35 cases sequentially in
fresh workspaces and retains per-run summaries, logs and exact commands.

After the complete matrix passes, consolidate it with:

```powershell
python tests/experiments/debugging/collect_steps_fault_matrix.py --matrix build/sn016/steps-fault-matrix-05 --output docs/experiments/evidence/E-05-steps-fault-matrix-summary.json
```

The collector requires 35 passed cases, matching experimental source/binary
hashes, fresh attach at zero, lifecycle cleanup and the exact default reference.
It also records all four failed matrix predecessors. Output must not already
exist; earlier evidence is never replaced.

## Measured integration result (2026-09-10)

All 35 IDE sessions in `build/sn016/steps-fault-matrix-05` passed:

- Three initially paced arbitration fault/recovery pairs for each of four faults
  (24 sessions), with exactly seven retained joint checkpoints in every failure
  and nine checkpoints plus one ADC transfer in every recovery.
- One continuously paced extended fault/recovery pair for each fault (8 sessions),
  with recovered SIGNAL inspection and firmware ADC code 3541.
- Extended paced lifecycle (two complete ten-checkpoint sequences and zero-state
  recreation/reconnect), shorter initially paced arbitration, and exact default
  guarded lifecycle controls (3 sessions).

The complete experimental matrix used matching sources/binaries. Every IDE
accepted automatic closure, exited zero and removed owned backend/relay
listeners. The default lifecycle stop/circuit records exactly matched
`build/sn016/resume-normal-04`. Fresh joint attaches started at zero.

| Arbitration failure | Host diagnosis range (ms) | Limit (ms) |
| --- | ---: | ---: |
| Analog process loss | 196.9855–268.6785 | 2000 |
| Live analog response timeout | 2199.4755–2222.1622 | 5000 |
| CPU backend loss | 159.6217–179.7227 | 2000 |
| Debugger loss | 253.1786–291.2418 | 2000 |

All 14 relay/accounting tests passed, including denied-read/no-acknowledgement
and unchanged deadline. No natural result-read retry occurred in the final
matrix; the retry branch itself has controlled unit evidence, while the real
matrix proves the combined profile still runs. The four earlier failed matrix
attempts remain failed, with no eighth joint checkpoint or verified notification.

[Consolidated evidence](../../../docs/experiments/evidence/E-05-steps-fault-matrix-summary.json)
contains all 35 results, shared hashes and predecessor failures. This completes
the bounded extended fault/arbitration integration with **initial pacing**;
fully unpaced interruption remains unapproved. SN-016 still needs the final
capability decision and full gate assessment before SN-017.

The changed bridge also passed a final direct-GDB regression: three recreated
lifecycle repetitions (six sequences, 54 fixed checkpoints and six host pauses)
in `build/sn016/plain-joint-pause-after-matrix-01`. Shared source/binary hashes
match the IDE matrix, with stable inspection, ADC 3541, zero exits and removed
listeners. See [direct regression evidence](../../../docs/experiments/evidence/E-05-steps-fault-plain-regression-summary.json).
