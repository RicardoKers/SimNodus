# Native register-pair request correlation

Add `--native-inspection-request` to [final inspection](INSPECTION.md) and retain
its prerequisites. The harness selects `enable-inspection-request` before worker
startup. After timed readback it sends `arm-inspection`, executes the returned
`inspection_request.command`, and submits the raw result through
`inspection-result "<MI result>"`. First acceptance returns the second command;
only the matching second result with equal r0/sp/lr values verifies inspection.
See [ADR 0035](../../docs/decisions/0035-native-register-pair-correlation.md).

`inspection_step` is 0 before arming, 1 while awaiting the first result, 2 while
awaiting the second, and 3 after verification. Explicit abort clears the step but
leaves the session failed. The original mailbox deadline applies to arming and
both results. Neither arming nor first acceptance commits or renews that deadline.
The host waits 100 ms between commands; the runner does not yet enforce that gap.

## Reproduction

Use the verified setup and command from [ADC process](ADC_PROCESS.md), adding
`--native-rc --native-readback --native-readback-mi --native-readback-request
--native-readback-time --native-inspection --native-inspection-request` and a
fresh output directory. This cycle records
`build/sn017/inspection-request-guarded-01`, four separate
`inspection-request-<fault>-loss-01` reports and `inspection-request-recovery-01`.
Fault runs select `--fault cpu`, `analog`, `cpu-owner` or `analog-owner` and omit
incompatible steps/lifecycle/pause/guarded flags. Reports and GDB comparisons are
in the [evidence](../../docs/experiments/evidence/SN-017-inspection-request-summary.json).

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/inspection_request_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/inspection-request-cases-03
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Choose unused output directories when repeating. The parser has 132 checks.
Fifteen process cases cover ordering, tokens, changed values and original-budget
expiry. `cases-01` and `cases-02` preserve failed test-setup attempts; `cases-03`
uses unconditional valid mailbox setup before inspection fault injection.
Real backend faults occur before pair arming; controlled pipe cases independently
cover register-pair failures. Session-scoped tokens are not host authentication.
Preserve PDF suppression, startup PID retry, the allowlist and all prior evidence.

The subsequent [interval slice](INSPECTION_INTERVAL.md) optionally withholds the
second request until native steady-clock elapsed time reaches 100 ms. It replaces
the pair's host-only delay without renewing the shared acceptance budget.
