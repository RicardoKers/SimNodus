# Composite native fixture grant transitions

Eighth SN-017 cycle; see [ADR 0021](../../docs/decisions/0021-composite-fixture-grant-transitions.md).
Select `--native-grants` with `--native-debug-results --native-commands --native-results`.

| Command | Transition |
| --- | --- |
| `grant-native 5000000 0 "absolute/path"` | Begin fixed grant and emit start. |
| `grant-native 100000000 1 "absolute/path"` | Begin explicitly paced grant and emit start. |
| `stop-native observed_ns` | Validate ready/progress/order and observed time; arm result reader and emit cancellation. |
| `poll-ready`, `poll-result` | Existing acknowledgement paths and deadlines, unchanged. |

The host no longer issues separate begin/start or observed-stop/cancel commands
on this path. Native debug progress still updates observation independently.
Counter evidence records only successful writes. Partial failures can leave a
begun grant or observed timestamp in the terminal snapshot, but never create an
acknowledgement or joint commit. A fresh session is required after failure.

## Reproduction

Use the [existing build setup](README.md) and fresh output directories:

```powershell
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/grant_transition_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/grant-transitions-01
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --process-runner build/sn017/native/Debug/simnodus_fixture_process.exe --native-results --native-commands --native-debug-results --native-grants --steps --lifecycle --pause --guarded --output build/sn017/grant-recovery-01
```

Reference control omits only `--native-grants`. Real failure controls replace
extended pause flags with `--fault cpu`, `analog`, `cpu-owner` or `analog-owner`.
The latter are fixed-checkpoint failure tests, not arbitrary debugger workloads.
Twenty-one pipe cases exercise fixed/paced grants, malformed/stale/partial ready,
expired ready, broken pipes, duplicate transitions, invalid duration, unpaced
100 ms and negative/out-of-grant stops. Prior ingress and command suites remain
required. Raw reports and hashes are in the
[evidence](../../docs/experiments/evidence/SN-017-grant-transitions-summary.json).

Next extract bounded analog catch-up after CPU acknowledgement. Preparation,
GDB and final scheduling remain in the harness; complete orchestration is pending.

## Subsequent analog command extraction

[Native analog commands](ANALOG_COMMANDS.md) add direct catch-up emission after
CPU acknowledgement. Replies remain host-read; exchange and commit stay separate.
