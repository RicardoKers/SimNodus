# Native raw stopped-time validation

Add `--native-readback-time` to the [mailbox request](READBACK_REQUEST.md) command
and retain its prerequisites. The runner's `arm-readback` response includes
`time_token` and `time_command` in addition to the memory request. The host sends
that monitor command after the memory read, keeping the original 1900 ms budget.
It forwards the unchanged memory result, elapsed-time stream and completion via
`readback-timed "<memory>" "<time stream>" "<completion>"`.

The adapter validates exact framing, decimal time fields and matching completion
token, then converts to nanoseconds for the existing final mailbox gate. No
normalized timestamp is accepted in this mode. Time must equal 4027000 ns in the
validated fixture, with prepared ADC code 3541 in real integration. Verification
does not commit. See [ADR 0033](../../docs/decisions/0033-native-raw-stopped-time-validation.md).

## Reproduction

Use the verified setup and command from [ADC process](ADC_PROCESS.md), adding
`--native-rc --native-readback --native-readback-mi --native-readback-request
--native-readback-time` and a fresh output directory. This cycle records
`build/sn017/readback-time-guarded-01`, four separate
`readback-time-<fault>-loss-01` reports and `readback-time-recovery-01`. Fault runs
select `--fault cpu`, `analog`, `cpu-owner` or `analog-owner` and omit incompatible
steps/lifecycle/pause/guarded flags. Input hashes, raw reports and GDB comparisons
are preserved in the [evidence](../../docs/experiments/evidence/SN-017-readback-time-summary.json).

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/readback_time_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/readback-time-cases-01
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Choose unused output directories for repeats. The time target covers 72 checks;
the process suite covers 22 cases including changed time, malformed stream,
missing/error/wrong completion, normalized-time bypass and shared-deadline expiry.
Earlier readback modes remain regression controls. Real backend loss occurs
before the final request; process cases separately verify invalid final input.

Output stream lines are untagged, so host request association remains necessary.
Only selected elapsed-time and completion records are validated natively; this
does not parse every monitor diagnostic or establish host authenticity. Preserve
the GDB allowlist, PDF suppression, startup PID retry and all earlier evidence.

The subsequent [inspection slice](INSPECTION.md) optionally adds native final
r0/sp/lr equality as a commit prerequisite, using the same acceptance deadline.
Host collection and observation-interval ownership remain unchanged.
