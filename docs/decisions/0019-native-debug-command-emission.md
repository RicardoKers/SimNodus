# ADR 0019: native bounded debug command emission

Date: 2026-09-10. Status: accepted for the SN-017 fixture extraction slice.

## Context

ADR 0018 extracted backend lifecycle. Progress sampling and joint notification
still emitted monitor text from Python. Moving those two commands is a small
coherent boundary; extracting their non-atomic response files requires a separate
publication/read contract. The existing harness remains the measured reference.

## Decision

Extend the opt-in inherited control channel with only `SampleCancellationProbe`
and `NotifyCancellationProbe`. Accept fresh absolute paths restricted to the
existing monitor-safe ASCII alphabet; reject existing result/error files. This
is a disposable owned-fixture transport, not a secure arbitrary project API.

Sampling requires native ready and a running grant. One sample may be outstanding;
the host supplies its observed timestamp through the existing checked `observe`
transition before cancellation. Observation is never CPU acknowledgement.

Notification requires CPU cancellation acknowledgement and analog agreement.
Its timestamp comes from native acknowledged state, never a caller-supplied
number. Only one notification is emitted per grant. Host `notified` attests the
exact bridge response; commit is rejected while that attestation is pending.
Notification emission and attestation must both occur within 1900 ms after the
successful cancellation write, without renewing that deadline. The existing
host checks the complete MI pause acknowledgement against 2000 ms and verifies
SIGINT/time plus stable inspection before commit.

Python still reads progress and notification files and validates actual GDB
notification. The native helper does not independently authenticate those host
attestations. Existing non-atomic file publication and read races remain to be
addressed in the next ingress slice. A notification already sent to GDB cannot
be withdrawn if later response reading or inspection fails; such failure aborts
without a new joint commit. No rollback or synchronous pause is inferred.

## Evidence and consequences

See the [protocol](../../tests/headless/DEBUG_COMMANDS.md) and
[evidence](../experiments/evidence/SN-017-debug-commands-summary.json).
Real Renode/ngspice/GDB runs exercise the native wire commands. OS-pipe tests
cover malformed paths, stale replies, ordering, duplicate commands, broken pipes
and expired deadlines; these tests are not substitutes for engine integration.
ADR 0014 capabilities and the IDE PDF suppression are unchanged. SN-017 remains
in progress; response ingress, fixture preparation and orchestration are pending.
