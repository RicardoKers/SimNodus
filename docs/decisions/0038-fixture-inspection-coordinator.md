# ADR 0038: Bounded fixture inspection coordinator

Date: 2026-09-12. Status: accepted for the SN-017 extraction slice.

## Decision

Move final readback and register/time inspection state from the session CLI into
`src/application/fixture_inspection.hpp`. `FixtureInspection` privately owns
activation modes, tokens, the original deadline, register observations, interval
state and verification flags. It handles the existing final-validation commands,
normalized legacy readback, pending-operation policy, commit prerequisite and
JSON diagnostics. The CLI delegates these responsibilities while retaining
backend transport, session ownership, global pending checks, abort and actual
joint commit scheduling.

Place this fixture-specific composition in application, because it combines
GDB adapter parsers with core acceptance rules. Core/domain do not acquire GDB,
Qt or third-party types. This is a protocol-aware native coordinator, not a
generic backend API or a complete application session. Unknown commands return
unhandled without consuming their arguments. The caller supplies current ADC
facts and whether worker activation is allowed; engine handles stay outside.

Preserve ADRs 0030-0037: the same opt-in modes, raw strings, tokens, rejection
rules, 100 ms interval and original 1900 ms deadline. JSON field names and values
are preserved; object field order changes. No new debugger command, transport,
capability or automatic joint commit is introduced. Clear pending state only
alongside explicit session abort; failed sessions remain terminal.

## Validation and limits

[Evidence](../experiments/evidence/SN-017-inspection-coordinator-summary.json)
and [reproduction](../../tests/headless/INSPECTION_COORDINATOR.md) retain the
real-engine matrix and earlier protocol regression suites. A direct C++ test
checks command delegation without argument consumption, independent coordinator
instances, normalized-mode isolation, commit prerequisites and terminal failure.
The new header is included in the real harness source hashes.

Host GDB transport, association of untagged time streams and fixture orchestration
remain necessary. Tokens do not authenticate the host. ADRs 0014/0027/0028 and
all physical/peripheral/unpaced restrictions remain unchanged. SN-017 remains
in progress. Next extract bounded ADC preparation/helper coordination from the
CLI into an application coordinator, retaining the measured helper transport,
single-transfer rule, deadlines and process cleanup.
