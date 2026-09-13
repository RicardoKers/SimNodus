# Extracted Renode execution coordinator

`FixtureExecution` in [application](../../src/application/fixture_execution.hpp)
owns the bounded execution/debug state and borrows the CLI-owned control channel.
See [ADR 0041](../../docs/decisions/0041-fixture-execution-coordinator.md).
[Command](COMMAND_CHANNEL.md), [result](RESULT_INGRESS.md),
[grant](GRANT_TRANSITIONS.md) and [debug ingress](DEBUG_INGRESS.md) protocols remain
unchanged; existing adapters still own their parsing and transport behavior.

## Reproduction

Use the real-engine command and native prerequisites in
[post-pair inspection](INSPECTION_TIME.md), with fresh
`build/sn017/execution-coordinator-*` output directories. This cycle records
`guarded-01`, four `<fault>-loss-01` controls and `recovery-01`. Successful runs
retain steps/lifecycle/pause/guarded; fault runs omit those flags and select the
existing cpu, analog, cpu-owner or analog-owner fault.

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/command_channel_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/execution-coordinator-cpu-command-01
python tests/headless/result_ingress_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --reference build/sn017/analog-coordinator-recovery-01/0/grant-0.txt --output build/sn017/execution-coordinator-cpu-ingress-02
python tests/headless/grant_transition_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/execution-coordinator-grant-01
python tests/headless/debug_command_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/execution-coordinator-debug-command-01
python tests/headless/debug_ingress_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/execution-coordinator-debug-ingress-01
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Select unused directories when repeating. Result ingress requires a raw grant
file, not a summary JSON. The first attempt omitted the reference; the next used
the wrong format and produced the preserved failed `cpu-ingress-01` report.
The corrected `cpu-ingress-02` run passed all 15 cases. Seventeen command,
21 grant, 21 debug command and 33 debug ingress cases also passed, alongside the
previous 251 cases: 358 total. Thirteen CTest targets and 18 Python tests passed.

[Evidence](../../docs/experiments/evidence/SN-017-execution-coordinator-summary.json)
retains report bodies, failed setup, source hashes and real-engine comparisons.
Six recovery sessions passed 54 fixed checkpoints, 60 RC checks and six joint
pauses with ADC 3541. Four loss controls retained two commits. Final execution
counters match except the host-timed progress polling count (one or two samples).
PDF suppression, startup PID retry, adapters and the other coordinators remain
unchanged. Host GDB transport and all bounded-profile restrictions remain.
