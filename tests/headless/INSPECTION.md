# Native final register stability

Add `--native-inspection` to [raw timed readback](READBACK_TIME.md), retaining its
prerequisites. The harness selects `enable-inspection` before initial worker
consumption. It forwards the two existing final token-93 register results through
`verify-inspection "<before>" "<after>"` after native timed readback succeeds.
No additional GDB read is introduced by this slice.

The adapter parses only the measured r0/sp/lr result shape and compares the
three 32-bit values. The runner requires final analog-ready state, successful
readback and the original mailbox deadline. Success sets `inspection_verified`;
a separate final commit then requires both flags. See
[ADR 0034](../../docs/decisions/0034-bounded-final-register-stability.md).

## Reproduction

Use the verified setup and full command from [ADC process](ADC_PROCESS.md), adding
`--native-rc --native-readback --native-readback-mi --native-readback-request
--native-readback-time --native-inspection` and a fresh output directory. This
cycle records `build/sn017/inspection-guarded-01`, four separate
`inspection-<fault>-loss-01` reports and `inspection-recovery-01`. Fault runs use
`--fault cpu`, `analog`, `cpu-owner` or `analog-owner` and omit incompatible
steps/lifecycle/pause/guarded flags. Input hashes, raw reports and GDB comparisons
are in the [evidence](../../docs/experiments/evidence/SN-017-inspection-summary.json).

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/inspection_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/inspection-cases-01
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Choose unused output directories when repeating. The parser covers 127 checks.
Eleven process cases cover valid comparison, change in each register, malformed
before/after records, absent/early/duplicate/late verification and extra arguments.
They assert failed verification cannot commit. Real backend losses precede
readback; controlled process cases separately cover final inspection failures.

The host still owns the two reads, their association and 100 ms separation.
Native comparison cannot prove that two supplied identical frames are distinct
observations. Preserve host PC/mailbox/time/analog stability checks, the GDB
allowlist, PDF suppression, startup PID retry and all historical evidence.

The subsequent [request slice](INSPECTION_REQUEST.md) optionally replaces the
host-supplied pair with two sequential native register requests and distinct
tokens. The minimum observation interval remains host-enforced.
