# SN-017 CPU and joint session contract slice

The C++20 `JointSession` in `src/core/joint_session.hpp` now validates live
transitions when the E-05 harness receives `--session-runner`. No third-party
engine or Qt types enter the contract. See [ADR 0015](../../docs/decisions/0015-incremental-session-contract.md).

## States and ownership

The helper replies with a JSON snapshot after every command. Numeric phases are
0 stopped, 1 granted, 2 observed, 3 CPU acknowledged, 4 analog ready and 5 failed.
The snapshot retains grant duration, generation, latest observed CPU time,
latest acknowledged CPU time, jointly committed time, discarded time from the
last acknowledgement, and separate acknowledgement/commit counts.

`begin <duration-ns> <paced-0-or-1>` must precede actual CPU authorization;
`observe <ns>` records a measured stop/progress point; `ack <start> <end>
<requested> <unused> <cancelled-0-or-1> <sink>` validates cancellation;
`analog <requested-ns> <actual-seconds> <volts>` validates endpoint agreement;
`commit` follows successful fixture exchange/inspection. `abort` is terminal;
`quit` is coordinated helper teardown. Unknown commands, reset, malformed
integers, unsupported grants and out-of-order transitions fail closed. EOF is
not successful teardown. The helper must be explicitly started with
`--cooperative-fixture` and the 100 ms grant requires pacing selection.

The host still supplies and verifies real engine artifacts, cancellation reason,
GPIO/ADC semantics, pacing and deadlines. Selecting a flag does not make an
unpatched backend cooperative. The contract does not expose general execution,
rollback, reset-in-place or general causal feedback. The line protocol is a
temporary test boundary; production transport and process ownership remain open.

## Reproduction

Build with the commands in [the RC slice protocol](README.md), then:

```powershell
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --fault cpu --output build/sn017/session-cpu-loss-NEW
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --fault analog --output build/sn017/session-analog-loss-NEW
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --steps --lifecycle --pause --guarded --output build/sn017/session-recovery-NEW
python tests/headless/session_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --reference build/sn017/session-recovery-NEW/summary.json --output build/sn017/session-adversarial-NEW
python -m unittest discover -s tests/experiments/debugging -p 'test_*.py'
```

Use fresh output directories. Omitting `--session-runner` preserves the earlier
control path. The [cycle evidence](../../docs/experiments/evidence/SN-017-session-contract-summary.json)
contains raw summaries and hashes. ngspice 47, the patched Renode 1.16.1 baseline,
GDB 15.2.90.20241229 and the owned firmware remain selected. No actual IDE startup
was changed or rerun; the release-note/PDF suppression source is unchanged.

## Measured result and limits

The final guarded recovery run passed three repetitions, each with fresh and
recreated engines: six sessions, 54 fixed checkpoints and six retained MI pauses.
Each C++ session recorded ten CPU acknowledgements and ten commits, including
its pause. Fixed CPU/GPIO/register/mailbox states match the prior RC extraction;
maximum voltage difference was 2.17604e-14 V, below 10 uV. ADC remained 3541.
All helper/backend/debugger processes exited zero and listeners were removed.

The real CPU-loss injection occurs at the third observed stop, before cancellation
acknowledgement: two acknowledgements/two commits remain. The real analog-loss
injection occurs after third CPU acknowledgement: three acknowledgements/two
commits remain. Neither failed path transfers ADC or records a pause. Both
terminate the C++ session and fresh recovery succeeds. These are two specific
failure boundaries, not a rerun of the E-05 IDE arbitration matrix.

The initial guarded run also passed before additional fault/count assertions;
its evidence is retained separately. Fifteen CLI cases replay its real commands
and test accounting corruption, missing/duplicate acknowledgement, unsupported
pacing, parser rejection, lost host and preservation of prior commits.
Both CTest targets and all 14 existing relay/accounting tests passed.
Three default controls without the gate preserve the earlier fixed path.

SN-017 remains in progress. Next extract the Renode cancellation-result parsing
and bounded transport/deadline ownership, then move orchestration incrementally.
Keep the current harness as a differential reference; do not broaden capability
claims or remove old evidence during that migration.


## Subsequent extraction

The [native result-ingress cycle](RESULT_INGRESS.md) now owns opt-in cancellation
file parsing and its read deadline in C++; the session cycle above remains its
historical baseline. Command transport and overall orchestration remain pending.

The subsequent [application session extraction](APPLICATION_SESSION.md) moves
global dispatch and pending/commit gates out of the CLI while preserving this
protocol and its separate acknowledgement/commit states.
