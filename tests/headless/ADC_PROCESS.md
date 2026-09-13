# Native ADC helper execution and ingress

Thirteenth SN-017 cycle; see [ADR 0026](../../docs/decisions/0026-native-adc-helper-process.md).
Select `--native-adc-process` with earlier native flags. The helper receives
`--adc-helper absolute-path --adc-port port` only from explicit fixture setup.

| Command | Native effect |
| --- | --- |
| `prepare-adc` | Existing bounded preparation and 1900 ms deadline. |
| `apply-adc` | Launch one contained `helper port adc 0 prepared_uv` process. |
| `poll-adc` | Bounded drain, process/result/cleanup validation; confirmation only after success. |
| `abort` | Terminate and reap the owned helper tree; retain prior joint commits. |

Snapshots expose process status, PID, exited/exit/cleaned fields and stdout/stderr
hex captures. Exit code is meaningful only when `adc_helper_exited` is true.
`adc_control` is returned only after native confirmation. Late or failed results
cannot commit. The host still controls polling and validates final firmware/GDB
state. Empty stderr is part of this exact helper profile. No arbitrary command
forwarding or executable discovery is added.

## Reproduction

Use the [existing build setup](README.md) and fresh output directories:

```powershell
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/adc_process_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --helper build/sn017/native/Debug/adc_process_fixture.exe --output build/sn017/adc-process-cases-02
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --process-runner build/sn017/native/Debug/simnodus_fixture_process.exe --native-results --native-commands --native-debug-results --native-grants --native-analog --native-analog-results --native-exchange --native-adc --native-adc-process --steps --lifecycle --pause --guarded --output build/sn017/adc-process-recovery-01
```

Control omits only `--native-adc-process`. Four real engine/supervisor loss controls
replace extended pause flags with `--fault cpu`, `analog`, `cpu-owner` or
`analog-owner`; those failures precede ADC invocation. Twenty-one actual process
cases cover the ADC-specific lifecycle and protocol, using a controlled test
executable, not pretending to validate Renode. The real integration uses the
unchanged measured `e05_control.exe`. Runner/descendant loss is independently
verified through process handles. Previous coordination/transport/core regressions
remain required.

See [results and preserved raw reports](../../docs/experiments/evidence/SN-017-adc-process-summary.json).
The five-second failure cleanup allowance is separate from the non-renewing
1900 ms success deadline. The helper's microsecond/no-readback limitations remain.
Next extract bounded analytical RC checks; full native GDB/commit scheduling is
still pending. PDF suppression and the previous startup retry are unchanged.

The subsequent [RC trajectory slice](RC_TRAJECTORY.md) can validate native
analog advance replies before ADC preparation. Helper execution, deadlines and
job cleanup retain the contracts above.

The subsequent [ADC coordinator extraction](ADC_COORDINATOR.md) moves transfer
state and helper ownership out of the CLI while preserving this protocol.
