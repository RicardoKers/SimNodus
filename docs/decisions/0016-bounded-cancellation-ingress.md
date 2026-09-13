# ADR 0016: bounded cancellation-result ingress

Date: 2026-09-10. Status: accepted for the SN-017 bounded extraction slice.

## Context

The C++ session contract previously received integers parsed by Python. The
owned Renode bridge publishes seven fields through a temporary file followed
by rename. Missing or temporarily unreadable results must not acknowledge a
CPU grant, and retry must never extend the original deadline.

## Decision

Extract the strict parser and local cancellation-result reader into the Renode
adapter. Opt in through `--native-results` together with `--session-runner`.
The C++ helper arms the reader before the host sends cancellation, then polls
without replacing its steady-clock deadline. It acknowledges the CPU only after
parsing succeeds and the existing session accounting accepts the result.

Limit the file to 4096 bytes and exactly seven unique fields. Accept unsigned
integer nanoseconds, one sink, reason `cancelled` and flag `True`. Accept LF or
CRLF and an optional final newline. Reject missing/duplicate/unknown fields,
overflow, signs, extra sinks, unsupported reasons and trailing value text.
The reader rejects an existing result when armed and ignores `.tmp` files.
An `.error` sidecar is a backend failure; it is not executable input.

Windows read access/sharing/lock failures remain pending within the same budget.
Use a 1900 ms native budget inside the unchanged 2000 ms host envelope, reserving
100 ms for IPC/diagnosis. Check the deadline before and after reading/parsing.
A response completed after the deadline cannot acknowledge the CPU. This is a
bound on acceptance and local polling, not a promise that arbitrary filesystem
or operating-system stalls are interruptible; the separate helper permits the
host watchdog and teardown to bound its own wait.

Native diagnostic states distinguish pending, ready, timeout, malformed,
backend error, I/O error, stale and consumed. Errors terminate the C++ session
without a new joint commit. Fresh processes remain the only recovery path.

## Alternatives

Keep Python parsing as the default differential control during extraction.
A complete Renode command transport rewrite is deferred; stdin commands,
ready/progress/notification files and engine lifecycle remain in the harness.
No new network protocol or generalized SDK is selected by this decision.

## Consequences

The parser and result deadline now run in C++, while the host retains its overall
phase watchdog and independent fixture checks. The contract remains specific to
the owned single-CPU cancellation fixture; generic completed grants are not
accepted. The helper IPC explicitly uses UTF-8. There are no new dependencies,
backend patches, capability expansions or redistributed third-party artifacts.

The [protocol](../../tests/headless/RESULT_INGRESS.md) and
[evidence](../experiments/evidence/SN-017-result-ingress-summary.json) cover real
engines, native Windows sharing locks, malformed/late results, fault boundaries
and fresh recovery. The IDE startup/PDF correction is unchanged.

## Revisit criteria

Revisit the file ingress when native command transport and process ownership
are extracted. Additional stop reasons, multiple sinks, non-local storage,
arbitrary firmware or different publication protocols require new evidence.
ADRs 0014 and 0015 retain their capability and lifecycle restrictions.
