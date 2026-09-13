# ADR 0031: Native raw MI mailbox decoding

Date: 2026-09-11. Status: accepted for the SN-017 fixture slice.

## Decision

Add opt-in `--native-readback-mi`, requiring `--native-readback`. The host selects
`enable-readback-mi` before initial worker consumption, then sends
`readback-mi <observed_time_ns> "<raw MI result>"` at the final stable inspection.
The existing `readback` command with normalized words is rejected in this mode.
The previous normalized mode remains a reference path.

The GDB adapter parses only the measured memory result: token 91, `done`, one
memory range beginning at 0x20000000 with zero offset and end 0x20000020, and
exactly 64 hexadecimal content characters. It requires the exact measured field
order and punctuation, rejects extra/truncated frames and decodes eight
little-endian words into a temporary array. Invalid frames leave output unchanged.
Hex letters may be upper or lower case. This is not a general MI parser.
Timestamp parsing rejects negative signs, overflow and trailing data. The existing
native final mailbox, ADC-confirmation and separate commit gates remain in force.

The host retains the exact result line from each memory-read call together with
its requested address/count. The final path requires one result for the mailbox
request and forwards that unchanged line. Python continues independently decoding
and asserting the mailbox and checking before/after stability. Native decoding
uses the forwarded raw line, not Python's normalized words.

## Evidence and limits

See [protocol](../../tests/headless/READBACK_MI.md) and
[evidence](../experiments/evidence/SN-017-readback-mi-summary.json). Real-engine
commands are compared byte-for-byte with final GDB log records. Tests exercise
endianness, every truncation point, invalid hex positions, wrong address/range,
wrong token, errors, duplicate/trailing frames, normalized bypass and timestamp
conversion. Initial passing reports before strict timestamp parsing are retained
with their original hashes; final reports use the hardened parser.

The parser validates a fixed frame length after the host command has arrived;
it is not a bounded streaming transport or a GDB connection owner. The reused
MI token and host-provided time do not independently prove freshness, correlation
or origin. GDB collection, command deadlines and inspection stability remain
host-owned. No new debugger operation or MCU capability is introduced, and
ADRs 0014, 0027 and 0028 remain unchanged. Next extract bounded request/result
correlation for the mailbox while retaining host GDB ownership and the allowlist.
SN-017 remains in progress. No commit or publication is authorized.
