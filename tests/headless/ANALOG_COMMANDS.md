# Native analog catch-up command channel

Ninth SN-017 cycle; see [ADR 0022](../../docs/decisions/0022-native-analog-command-channel.md).
Select `--native-analog` with all native grant/debug/result flags. The session
helper receives a second explicitly inherited pipe via `--analog-stdin HANDLE`.
The harness creates the analog worker before duplicating its stdin.

| Operation | Contract |
| --- | --- |
| `advance-native` | Only after CPU acknowledgement; derive target from native state and write once. |
| `analog ns seconds volts` | Host-read response; require pending request, unchanged 1 ps endpoint/finite-value checks and 1900 ms response deadline. |
| `commit` | Existing gate; request emission and analog agreement do not commit. |

Snapshots add cumulative `advance_writes` and `analog_pending`. The parent and
native helper close their duplicate handles independently; the host retains its
own pipe for exchange/inspection/quit commands. There is no concurrent writer in
the measured harness ordering. Arbitrary concurrent writers are not supported.
The host still checks the analytical RC trajectory, reads/logs worker replies,
performs GPIO/ADC exchange and verifies stable inspection before commit.

## Reproduction

Use the [existing build setup](README.md) and fresh output directories:

```powershell
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/analog_command_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/analog-command-pipes-01
python tests/headless/startup_pid_regression.py --output build/sn017/analog-command-startup-01
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --process-runner build/sn017/native/Debug/simnodus_fixture_process.exe --native-results --native-commands --native-debug-results --native-grants --native-analog --steps --lifecycle --pause --guarded --output build/sn017/analog-command-recovery-02
```

Control omits only `--native-analog`. Four real engine/supervisor loss controls
replace extended pause flags with `--fault cpu`, `analog`, `cpu-owner` or
`analog-owner`. Thirteen new pipe cases cover ordering, duplicate/unsolicited/late
responses, invalid endpoints, nonfinite input, broken pipes, caller-supplied target
rejection and a subsequent grant. Four actual PID file/lock cases and nine
supervisor lifecycle cases validate the incidental startup-read fix. Existing
native grant, debug, result and core suites remain required.

The failed first recovery report is preserved, together with successful final
recovery, controls, hashes and measurements in the
[evidence](../../docs/experiments/evidence/SN-017-analog-command-summary.json).
Next extract complete bounded analog reply ingress; command success alone is
not worker agreement or joint commit. IDE startup/PDF code was not changed.

## Subsequent reply extraction

[Native analog ingress](ANALOG_INGRESS.md) adds exclusive stdout ownership and
validates initial, advance and inspection/exchange frames in the runner.
