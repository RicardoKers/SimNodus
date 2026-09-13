# Extracted ADC coordinator

`FixtureAdc` in [src/application](../../src/application/fixture_adc.hpp) owns the
bounded ADC transfer state and the existing helper adapter. See
[ADR 0039](../../docs/decisions/0039-fixture-adc-coordinator.md). The
[ADC process protocol](ADC_PROCESS.md) and final
[inspection protocol](INSPECTION_COORDINATOR.md) are unchanged.

## Reproduction

Use the full real-engine command and native prerequisites from
[final inspection](INSPECTION_TIME.md), with fresh
`build/sn017/adc-coordinator-*` output directories. This cycle records
`guarded-01`, four `<fault>-loss-01` controls and `recovery-01`. Successful runs
retain steps/lifecycle/pause/guarded; fault runs omit those four flags and select
`--fault cpu`, `analog`, `cpu-owner` or `analog-owner`.

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/adc_process_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --helper build/sn017/native/Debug/adc_process_fixture.exe --output build/sn017/adc-coordinator-process-01
python tests/headless/adc_coordination_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/adc-coordinator-adc-01
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Use unused directories when repeating. Twenty-one process cases test the actual
Windows helper lifecycle; 18 coordination cases retain single-transfer and
legacy-confirmation coverage. Earlier inspection, mailbox, session and RC pipe
suites are also rerun. The direct `fixture-adc-coordinator` CTest checks instance
isolation, delegation and denied/duplicate transitions without launching engines.
Real integration is established by the separately recorded engine matrix.

[Evidence](../../docs/experiments/evidence/SN-017-adc-coordinator-summary.json)
retains report bodies, paths, source hashes and previous-cycle comparison.
Keep all earlier reports, PDF suppression and startup PID retry. The same helper
adapter performs cleanup; host GDB transport and untagged-stream association
remain necessary. This fixture path does not validate physical ADC acquisition.
