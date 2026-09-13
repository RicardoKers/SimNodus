# Native complete debug replies

The seventh SN-017 cycle implements [ADR 0020](../../docs/decisions/0020-native-debug-reply-ingress.md).
Use `--native-debug-results` together with `--native-commands --native-results`.
The runner now reads progress and notification files itself. Python polls JSON
snapshots and retains actual GDB verification and final orchestration.

| Command | Native effect |
| --- | --- |
| `sample-ingress "absolute/path"` | Arm fresh reader, emit progress request; share deadline across samples in the grant. |
| `notify-ingress "absolute/path"` | Arm reader with existing cancellation deadline, emit acknowledged joint time after analog agreement. |
| `poll-debug` | Retry within deadline; validate complete reply and update observation or notification gate. Never commit. |

Snapshots add `debug_status` and `debug_retries`. Pending file reads cannot be
bypassed by host observation/notification attestations. Successful notification
consumption does not prove GDB inspection; the existing host verifies that before
commit. Exclusive Windows reads require the bridge's single-open publication
behavior. Closed empty/prefix files fail; open partial writers remain pending.

## Reproduction

Build with the [existing setup](README.md), then use fresh output directories:

```powershell
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/debug_ingress_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/debug-ingress-files-03
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --process-runner build/sn017/native/Debug/simnodus_fixture_process.exe --native-results --native-commands --native-debug-results --steps --lifecycle --pause --guarded --output build/sn017/debug-ingress-recovery-01
```

Host-read control omits only `--native-debug-results`. Four real engine/supervisor
loss controls replace the extended pause flags with `--fault cpu`, `analog`,
`cpu-owner`, or `analog-owner`. They do not inject live debug-response faults.
The 33 file/pipe cases cover complete and partial writers, missing/late/locked
replies, invalid text, overflow, out-of-grant timestamps, stale/error replies,
duplicates, bypass attempts and non-renewing sample/notification deadlines.
Prior command, cancellation and state regressions remain required.

See [results and hashes](../../docs/experiments/evidence/SN-017-debug-ingress-summary.json).
Early passing reports precede the final shared-progress-deadline strengthening;
they remain preserved with their original hashes. Next consolidate bounded
fixture orchestration without expanding capabilities or removing the reference.

## Subsequent orchestration step

[Composite grant transitions](GRANT_TRANSITIONS.md) consolidate begin/start and
observed-stop/cancel while retaining these native readers and their deadlines.
