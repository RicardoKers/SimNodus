# Extracted analog worker coordinator

`FixtureAnalog` in [application](../../src/application/fixture_analog.hpp) owns
bounded worker sequencing and analog acceptance. It borrows the CLI-owned
channel/reader; existing adapters own their handles. See
[ADR 0040](../../docs/decisions/0040-fixture-analog-coordinator.md).
The [analog command](ANALOG_COMMANDS.md), [raw ingress](ANALOG_INGRESS.md),
[exchange](EXCHANGE_INSPECTION.md) and RC protocols are unchanged.

## Reproduction

Use the full real-engine command and native prerequisites in
[post-pair inspection](INSPECTION_TIME.md), choosing fresh
`build/sn017/analog-coordinator-*` output directories. This cycle records
`guarded-01`, four `<fault>-loss-01` controls and `recovery-01`.
Successful runs retain steps/lifecycle/pause/guarded; fault runs omit those four
flags and select `--fault cpu`, `analog`, `cpu-owner` or `analog-owner`.

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/analog_command_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/analog-coordinator-command-01
python tests/headless/analog_ingress_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/analog-coordinator-ingress-01
python tests/headless/exchange_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/analog-coordinator-exchange-01
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Use unused output directories when repeating. This cycle passed 13 analog
command, 25 ingress and 19 exchange cases, plus the preceding 194 protocol/process
cases: 251 in total. Twelve CTest targets and 18 Python unit tests passed.
The direct coordinator CTest exercises actual pipe ownership and rejected writes;
real backend integration is validated separately.

[Evidence](../../docs/experiments/evidence/SN-017-analog-coordinator-summary.json)
retains report bodies, hashes, final counter comparisons and GDB records. Six
recovery sessions passed 54 fixed checkpoints, 60 RC checks and six joint pauses
with ADC 3541. All four loss controls retained two joint commits. No new
capability is claimed. Preserve earlier evidence, PDF suppression, startup PID
retry and the existing helper/worker implementations.
