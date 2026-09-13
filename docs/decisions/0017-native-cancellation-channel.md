# ADR 0017: native bounded grant and cancellation channel

Date: 2026-09-10. Status: accepted for the SN-017 fixture extraction slice.

## Context

C++ already validates cancellation results and joint-state transitions, but the
Python harness still sends grant/cancel commands and interprets readiness. These
operations must share phase deadlines without introducing general monitor access
or claiming that writing a command establishes a CPU acknowledgement.

## Decision

Add an opt-in `--native-commands` path requiring `--native-results`. The host
passes one explicitly duplicated, inheritable Renode stdin pipe handle through
an explicit process handle list. The parent closes the duplicate after launch;
the C++ channel owns and closes the inherited copy. The original pipe stays with
the harness for still-unextracted commands. Calls are serialized by that harness;
concurrent writes from arbitrary callers are not supported.

The channel generates only `StartCancellationProbe` and `CancelCancellationProbe`.
A start uses the already granted duration, with the measured pacing suffix for
the 100 ms profile. ASCII absolute paths without whitespace or monitor syntax
are required, retaining the experimental path restriction. No caller-supplied
monitor command is forwarded. Unknown commands and invalid ordering fail closed.

Arm the ready reader before sending start. Empty or partial `active` content is
pending because the bridge's `.ready` write is not atomic; only complete `active`
is accepted. Existing result/ready/error files reject a new start before writing.
An error sidecar takes precedence over ready. Ready never acknowledges CPU time.

Cancel arms the existing result reader before writing. Its deadline covers both
write and result polling. Each ready or cancel/result phase has one 1900 ms
native budget under a 2000 ms host watchdog. Polling cannot renew either budget.
Debug execution between those phases is not governed by the ready deadline.
Synchronous OS pipe/file operations can stall; the host watchdog and fresh-process
teardown remain necessary, as in ADR 0016.

## Alternatives

Keep the Python command path as a differential control. Passing arbitrary text
through a generic monitor adapter was rejected for this slice: only the two
measured commands are needed. Native engine creation and general transport remain
pending rather than being bundled into this command extraction.

## Consequences

C++ now gates start, ready, cancel and result accounting. GDB relay, progress
sampling, joint notification, engine creation and teardown remain in the harness.
The helper does not become an independent scheduler. Physical ADC acquisition,
general unpaced execution, rollback and in-place reset remain unsupported.

The [protocol](../../tests/headless/COMMAND_CHANNEL.md) and
[evidence](../experiments/evidence/SN-017-command-channel-summary.json) cover real
engines, fault/fresh recovery, exact pipe bytes, order rejection, partial/late
ready and inherited-pipe closure. No engine patch or dependency was changed.
The IDE/PDF suppression code remains unchanged.

## Revisit criteria

Revisit handle transfer and serialized command ownership during native process
lifecycle extraction. Other monitor commands, concurrent writers, whitespace
paths, non-Windows handle transfer or arbitrary firmware require new evidence.
ADRs 0014–0016 retain their capability and causality restrictions.
