# Bounded native ADC preparation and confirmation

Twelfth SN-017 cycle; see [ADR 0025](../../docs/decisions/0025-bounded-adc-input-coordination.md).
Select `--native-adc` with all previous native options.

| Command | Gate and result |
| --- | --- |
| `prepare-adc` | Native analog-ready at 4010750 ns after native GPIO high; derive channel 0 integer microvolts and expected code. |
| `adc-applied channel uv before_us after_us` | Host reports successful helper application; require exact prepared input and 4010/4010 us before the original deadline. |
| `abort` | Terminal cleanup releases pending coordination without a new commit. |

Snapshots expose `adc_request`, `adc_pending` and `adc_confirmations` after
preparation. Expected code is validation metadata, not a quantized voltage sent
to the backend. The existing helper receives microvolts. Its response does not
read back channel/value and has only microsecond resolution. Actual GDB and
native CPU/analog checks remain responsible for the precise boundary evidence.
The old host-only calculation path remains a differential control.

## Reproduction

Use the [existing build setup](README.md) and fresh output directories:

```powershell
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/adc_coordination_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/adc-coordination-pipes-01
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --analog-runner build/sn017/native/Debug/simnodus_rc_fixture.exe --session-runner build/sn017/native/Debug/simnodus_session_contract.exe --process-runner build/sn017/native/Debug/simnodus_fixture_process.exe --native-results --native-commands --native-debug-results --native-grants --native-analog --native-analog-results --native-exchange --native-adc --steps --lifecycle --pause --guarded --output build/sn017/adc-coordination-recovery-01
```

Control omits only `--native-adc`. Four real engine/supervisor loss controls replace
extended pause flags with `--fault cpu`, `analog`, `cpu-owner` or `analog-owner`.
Eighteen actual pipe cases cover initial/wrong boundary, missing high, premature
preparation, invalid voltage, duplicate/mismatched/late confirmations and pending
commit. The new CTest target covers six numerical reference cases and four invalid
inputs. Earlier exchange, ingress, grant, debug and result regressions remain
required. The initial new test build lacked an explicit initializer-list include;
this was corrected before the passing numerical test run.

Exact results, hashes and raw reports are in the
[evidence](../../docs/experiments/evidence/SN-017-adc-coordination-summary.json).
Next extract bounded helper invocation/result ownership. A completed preparation
or confirmation does not establish joint commit or general ADC compatibility.

## Subsequent native invocation

The [native ADC process slice](ADC_PROCESS.md) now owns helper execution and
result ingress when `--native-adc-process` is selected. It shares the preparation
deadline and rejects host confirmation on that path. The earlier host invocation
remains a reference control.
