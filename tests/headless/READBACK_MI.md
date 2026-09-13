# Native raw MI mailbox ingress

Add `--native-readback-mi` to the validated [readback command](READBACK.md), keeping
`--native-readback` and its prerequisites. The harness selects
`enable-readback-mi` before initial worker consumption. The final command carries
the host-observed time and a quoted, unchanged MI result line. The adapter checks
token, address, offset, end, exact framing and 32 bytes of hex, decodes little
endian, then uses the existing mailbox verifier and prepared ADC state.
See [ADR 0031](../../docs/decisions/0031-native-raw-mi-mailbox-ingress.md).

The raw mode rejects normalized `readback` attestations. It accepts only the
measured GDB field format; generic MI variations and other memory regions are
outside scope. The host still associates the line with its request and supplies
time. Native token freshness and raw time ingress remain pending.

## Reproduction

Use the verified engine setup from [ADC process](ADC_PROCESS.md) with
`--native-rc --native-readback --native-readback-mi` and a fresh output directory.
This cycle records `build/sn017/readback-mi-guarded-01`, four separate
`readback-mi-<fault>-loss-01` reports and `readback-mi-recovery-01`. Fault runs use
`--fault cpu`, `analog`, `cpu-owner` or `analog-owner` and omit incompatible
steps/lifecycle/pause/guarded flags. The initial guarded report predates strict
timestamp conversion; final recovery and fault controls use the hardened parser.
All reports and input hashes remain in the
[evidence](../../docs/experiments/evidence/SN-017-readback-mi-summary.json).

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/readback_mi_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/readback-mi-cases-02
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Choose unused output directories to repeat. The parser target covers 218 checks;
23 process cases test valid frames, malformed fields and native ordering. Host
unit tests retain request identity and all candidate result lines, allowing the
harness to reject ambiguous results rather than selecting one silently.
Real faults precede readback; invalid final ingress is covered by process tests.
No host authenticity or independent freshness proof is claimed. Preserve the
existing PDF suppression, PID startup retry and all earlier evidence.

The subsequent [request slice](READBACK_REQUEST.md) optionally replaces the
reused token with one native-owned request and a shared acceptance deadline.
Exact mailbox framing and the separate final commit gate remain unchanged.
