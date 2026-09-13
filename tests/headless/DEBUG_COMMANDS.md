# Native debug command emission

This sixth SN-017 slice extends `--native-commands`; defaults retain the Python
reference. Read [ADR 0019](../../docs/decisions/0019-native-debug-command-emission.md).

| Session command | Effect and precondition |
| --- | --- |
| `sample-native "absolute/path"` | Emit one progress request after ready, before cancellation. |
| `observe ns` | Validate host-read observation and clear an outstanding sample. Does not acknowledge CPU. |
| `notify-native "absolute/path"` | Emit once after CPU acknowledgement and analog agreement, using the acknowledged timestamp. |
| `notified` | Host attests exact `notified` reply; clear pending notification within cancellation deadline. |
| `commit` | Existing state gate, additionally rejects pending notification. Harness first verifies actual SIGINT and stable inspection. |

Snapshots expose cumulative `sample_writes`, `notify_writes` and pending flags.
Successful writes do not imply backend execution. Transport/order/deadline failure
is terminal and retains earlier commits. Fixed checkpoint commits need no debug
notification. New grants clear per-grant flags, not cumulative counters.

Progress and notification response parsing remains in Python. Fresh path checks
are preflight checks, not atomic reservations against other processes. This slice
uses only trusted disposable fixture directories. The bridge's non-atomic file
publication requires a closure/complete-response contract in the next native
reader slice; this cycle does not claim to solve that race. General debugging,
unpaced interruption, arbitrary circuits and host-death supervision are unchanged.

## Reproduction

Build with the existing [RC setup](README.md). Run from the repository root:

```powershell
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/debug_command_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/debug-commands-01
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --process-runner build/sn017/native/Debug/simnodus_fixture_process.exe --native-results --native-commands --steps --lifecycle --pause --guarded --output build/sn017/debug-recovery-01
```

Always select a fresh output directory. Four engine/supervisor loss runs use the
same backend arguments with `--fault cpu`, `analog`, `cpu-owner`, or `analog-owner`
instead of `--steps --lifecycle --pause --guarded`. These are fixed-checkpoint
failure controls, not live notification-response fault injection. The 21 new
OS-pipe cases cover debug-command failures. Prior 17 command, 15 ingress and 15
session regressions plus 14 Python tests also passed. The legacy control omits
all extracted runner/native flags and passes three four-checkpoint repetitions.

Results and hashes are preserved in the
[evidence](../../docs/experiments/evidence/SN-017-debug-commands-summary.json).
The IDE launch matrix was not repeated: launch code and PDF suppression were
not changed. Next extract complete-response ingress before native orchestration.

## Subsequent ingress extraction

[Native debug reply ingress](DEBUG_INGRESS.md) adds an opt-in complete-response
reader. The host-read path documented above remains the differential reference.
