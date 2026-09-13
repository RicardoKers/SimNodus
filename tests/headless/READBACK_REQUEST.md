# Native mailbox request correlation

Add `--native-readback-request` to [raw MI ingress](READBACK_MI.md), retaining its
prerequisites. The harness selects `enable-readback-request` before initial worker
consumption. At the final analog-ready boundary it sends `arm-readback` and
receives `readback_request` with `token` and the exact `command`. No caller-supplied
address, token, size or timeout is accepted by arming.

The host executes the command, checks one matching result and unchanged virtual
time, then submits `readback-mi <time> "<raw result>"`. The runner checks the token,
frame, prepared ADC state, mailbox and original 1900 ms deadline. `readback_pending`
clears only on successful verification or explicit abort. Earlier failure stays
terminal; no pending or rejected result can commit. See
[ADR 0032](../../docs/decisions/0032-bounded-mailbox-request-correlation.md).

## Reproduction

Use the verified setup and full command from [ADC process](ADC_PROCESS.md), adding
`--native-rc --native-readback --native-readback-mi --native-readback-request` and
choosing a fresh output directory. This cycle records
`build/sn017/readback-request-guarded-01`, four separate
`readback-request-<fault>-loss-01` reports and `readback-request-recovery-01`.
Fault runs select `--fault cpu`, `analog`, `cpu-owner` or `analog-owner` and omit
incompatible steps/lifecycle/pause/guarded flags. Reports, hashes and real GDB
result comparisons are preserved in the
[evidence](../../docs/experiments/evidence/SN-017-readback-request-summary.json).

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/readback_request_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/readback-request-cases-02
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Choose an unused output directory when repeating. Fourteen request cases include
wrong and legacy tokens, unarmed result, duplicate arm/reply, 2.01 s timeout,
1.1 s delayed success, commit/inspection while pending, caller arguments,
early arm, normalized bypass and disabled mode. Initial `cases-01` remains
preserved; `cases-02` follows removal of unrelated inherited test branches.
Parser tests cover 224 cases; legacy raw and normalized modes remain regression
controls. Real backend loss controls occur before the mailbox request.

Tokens are reserved within one owned session and may repeat after recreation.
This is not host authentication or a cross-session replay defense. Native raw
time decoding remains pending. Keep the GDB allowlist, PDF suppression, PID
startup retry and all historical evidence unchanged.

The subsequent [raw time slice](READBACK_TIME.md) optionally adds a native time
command/token and validates raw elapsed-time/completion records under the same
request deadline. Host stream association remains required.
