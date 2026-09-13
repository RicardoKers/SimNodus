# ADR 0015: incremental C++ joint session contract

Date: 2026-09-10. Status: accepted for the SN-017 bounded extraction slice.

## Context

The persistent RC worker is extracted, but E-05 still owns engine transport,
GDB commands, pacing, inspection and lifecycle. Moving all orchestration at once
would obscure equivalence with the measured ADR 0014 behavior. A CPU cancellation
acknowledgement must remain distinct from analog agreement and joint publication.

## Decision

Extract an engine-independent `JointSession` state machine into the C++20 core.
Exercise it synchronously through an opt-in `simnodus_session_contract` helper
while the existing harness continues to schedule the real engines. The helper
is an extraction test boundary, not a selected production IPC architecture.
It neither launches engines nor provides a second virtual clock. Its timestamps
come from explicit host grants and measured backend results.

Require the bounded cooperative fixture selection, 5 ms fixed grants or paced
100 ms asynchronous grants, monotonic observations, single-sink cancellation
accounting, analog endpoint agreement and a separate commit command. The unused
portion is recorded as discarded and never reused as a new authorization.
The helper accepts only cancellation outcomes used by this fixture, not a
general run-to-completion API. The host remains responsible for artifact
verification, real pacing, deadlines, RC trajectory checks and successful
exchange/inspection before commit. Capability selection is a host assertion,
not automatic proof that an arbitrary backend supports cancellation.

Failure is terminal and retains the last acknowledged CPU time and joint commit.
There is no reset transition: fresh engines require a new state-machine instance.
The existing Python checks remain independent validation during migration.
Without `--session-runner`, the experimental path remains available as a control.

## Alternatives

A complete orchestration rewrite was deferred until the extracted contract has
real-backend evidence. Offline trace validation alone cannot block an invalid
live transition, so the selected path checks commands before proceeding.
The temporary helper process makes that gating observable without binding the
core to Python, Renode, ngspice or Qt APIs.

## Consequences

The slice adds a synchronous helper and records each transition; it does not
complete SN-017 or replace debugger packet authorization. A pause notification
is still sent only after CPU cancellation and analog agreement; the checkpoint
commit follows successful notification and stable inspection. Failure after
notification is not retroactively claimed to erase that notification.
Arbitration/fault timing coverage remains bounded by ADR 0014; the new CPU-loss
case is specifically after an observed stop and before cancellation confirmation.

The [protocol](../../tests/headless/SESSION_CONTRACT.md) and
[evidence](../experiments/evidence/SN-017-session-contract-summary.json) record
real-engine equivalence, two failure boundaries and fresh recovery. All code is
owned project material; no additional dependency or redistribution is introduced.

## Revisit criteria

Revisit the helper boundary when extracting native transport/orchestration.
Expand the contract only with measured new profiles; general unpaced operation,
rollback, in-place reset, arbitrary circuits and physical ADC acquisition remain
unsupported. This decision does not change ADR 0014 or the classroom targets.
