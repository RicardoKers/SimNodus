# ADR 0035: Native final register-pair request correlation

Date: 2026-09-12. Status: accepted for the SN-017 fixture slice.

## Decision

Add opt-in `--native-inspection-request`, requiring `--native-inspection`.
`enable-inspection-request` selects sequential native ownership of the final
register pair. After timed mailbox verification, `arm-inspection` returns the
fixed r0/sp/lr command and token 3000000 plus the session generation.
`inspection-result` accepts only the active token and exact register frame.
Acceptance of the first result stores its values and returns the second command
with token 4000000 plus generation. Only the matching second result with unchanged
values sets `inspection_verified`. The final commit remains separate.

The runner permits one pair per owned session, rejects duplicate arming, and
blocks other session operations while either result is pending except explicit
abort. Legacy `verify-inspection` cannot bypass the correlated mode. Both reads
and comparison must finish within the original mailbox acceptance deadline;
arming or accepting a first result does not renew the 1900 ms budget. Parser
output is staged, and token matching rejects prefixes, leading zeros and the
ordinary token-93 path.

The host executes the supplied commands unchanged, waits 100 ms between them,
and checks post-pair virtual time remains unchanged. It passes the remaining
budget to each operation. Existing earlier host inspection remains an independent
reference, so this mode introduces two additional permitted register reads. No
new GDB operation or allowlist entry is added.

## Evidence and limits

See [protocol](../../tests/headless/INSPECTION_REQUEST.md) and
[evidence](../experiments/evidence/SN-017-inspection-request-summary.json). Real
logs carry both native tokens in order. Tests cover missing/early requests,
duplicate arming/results, wrong tokens, first-result replay as second, changed
values, pending commits, legacy bypass and the shared deadline.
Two initial test attempts failed because inherited case names also changed
mailbox setup. Their reports remain preserved; final tests isolate setup from
inspection fault injection.

Tokens are scoped to one owned runner/GDB session and may repeat after full
recreation. They establish expected-response correlation, not host authenticity
or cross-session replay defense. The 100 ms observation interval remains
host-enforced; native acceptance currently does not impose a minimum gap.
GDB transport and other host stability assertions remain necessary. Next extract
bounded native observation-interval gating for the register pair without renewing
the shared deadline. SN-017 remains in progress; ADRs 0014, 0027 and 0028 are
unchanged. No commit or publication is authorized.
