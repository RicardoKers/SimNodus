# Native fixture high and inspection

Eleventh SN-017 cycle; see [ADR 0024](../../docs/decisions/0024-native-fixture-exchange-inspection.md).
Select `--native-exchange` with all earlier native options.

| Command | Gate and effect |
| --- | --- |
| `high-native` | Once, at 2011375 ns after CPU/analog agreement; arm reader and emit `high`. |
| `inspect-native` | Initialized worker, stopped or analog-ready; arm reader and emit `inspect`. |
| `poll-worker` | Existing reader validates an unchanged snapshot before releasing the pending operation. |

Snapshots expose cumulative successful `high_writes` and `inspect_writes`.
A successful write is not a validated reply. Pending replies prevent commit.
The host verifies actual GPIO state, CPU registers and stable inspection timing;
these commands do not independently establish those facts. ADC injection and
complete scheduling remain in the harness. The old worker-write path remains
available as a differential control.

## Reproduction

Use the [existing build setup](README.md) and fresh output directories:

```powershell
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/exchange_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/exchange-pipes-01
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --process-runner build/sn017/native/Debug/simnodus_fixture_process.exe --native-results --native-commands --native-debug-results --native-grants --native-analog --native-analog-results --native-exchange --steps --lifecycle --pause --guarded --output build/sn017/exchange-recovery-01
```

Control omits only `--native-exchange`. Four real failure controls replace extended
pause flags with `--fault cpu`, `analog`, `cpu-owner` or `analog-owner`. The 19
actual pipe cases cover initial/stopped inspection, premature commands, wrong
GPIO boundary, duplicates, pending commit, changed/missing/stale replies, broken
channels and caller arguments. Earlier ingress, grant, command and core suites
remain required. Native engine results were saved before a tool approval usage
limit interrupted test creation; the missing tests were completed on resumption.
No unexecuted test was counted as passing.

Exact counts, hashes, tolerances and raw reports are in the
[evidence](../../docs/experiments/evidence/SN-017-exchange-inspection-summary.json).
Next extract bounded ADC boundary-input coordination. Preserve all earlier
capability limits, the reference, PID retry and PDF suppression.

## Subsequent ADC coordination

[Bounded ADC coordination](ADC_COORDINATION.md) derives input from the accepted
analog sample and gates confirmation before commit. Helper invocation remains
host-owned.
