# Bounded application session

`FixtureSession` in [application](../../src/application/fixture_session.hpp)
composes the existing coordinators and owns joint state, dispatch and global
pending/commit gates. The CLI owns endpoints, startup and standard input/output.
See [ADR 0042](../../docs/decisions/0042-fixture-application-session.md).
All [execution](EXECUTION_COORDINATOR.md), [analog](ANALOG_COORDINATOR.md),
[ADC](ADC_COORDINATOR.md) and [inspection](INSPECTION_COORDINATOR.md) protocols
remain unchanged.

## Reproduction

Use the real-engine command and native prerequisites from
[post-pair inspection](INSPECTION_TIME.md), with fresh
`build/sn017/application-session-*` output directories. This cycle records
`guarded-01`, four `<fault>-loss-01` controls and `recovery-01`.
Successful runs retain steps/lifecycle/pause/guarded; fault runs omit those four
flags and use the existing cpu/analog/cpu-owner/analog-owner fault argument.

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/session_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --reference build/sn017/session-guarded-01/summary.json --output build/sn017/application-session-session-01
python tests/headless/result_ingress_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --reference build/sn017/execution-coordinator-recovery-01/0/grant-0.txt --output build/sn017/application-session-cpu-ingress-01
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Select unused directories when repeating. All previous 358 protocol/process
cases are rerun, together with 18 Python tests. Fourteen CTest targets include
a direct composed-session test using a real pending pipe, proving global guards
run before commit and quit. These checks do not substitute for engine evidence.

[Evidence](../../docs/experiments/evidence/SN-017-application-session-summary.json)
retains raw reports, source hashes and previous-cycle comparisons. Preserve
historic failures, PDF suppression and startup PID retry. GDB transport and
fixture progression remain harness responsibilities. SN-017 closure requires
an acceptance audit; no general firmware, physical ADC or unpaced capability
is added by this composition.
