# ADR 0043: Accept the bounded SN-017 headless extraction

Date: 2026-09-12. Status: accepted for the measured Windows x64 fixture composition.

## Decision

Close SN-017 against its recorded acceptance criterion, "Headless runner and
contract tests using real engines", for the ADR 0014 bounded profile. The
[acceptance audit](../experiments/SN-017-acceptance.md) maps the implementation and
evidence to that criterion. Native C++ core, adapters, process supervision and
application session are implemented and exercised by the explicit Python/C#
fixture driver. Fourteen CTest targets, 358 protocol/process cases, 18 Python
tests and the recorded real-engine matrix support this scope.

The accepted runner is this headless composition. Python still schedules the
fixture, drives GDB/MI and prepares/records experiments; those responsibilities
are not claimed as extracted native orchestration. A native-only standalone
runner was not a requirement of the backlog criterion. Do not redefine the
experimental helper protocol as the production IPC architecture.

This closes the bounded extraction, not the complete M2 application/runner,
production simulator or all earlier experiment modes. General causal feedback,
arbitrary firmware/circuits, physical ADC, unpaced execution, prediction and
rollback remain unsupported. No new MCU/peripheral, GUI, shared instrumentation
or distribution capability is approved. ADRs 0014/0027/0028 remain unchanged.

## Evidence and consequences

The [audit record](../experiments/evidence/SN-017-acceptance-summary.json) verifies
current source/binary hashes against the preceding real-engine cycle and retains
its report/log hashes. No new engine run or CubeIDE recertification is claimed
for this documentation audit. Preserve all historical failures, voltage-roundoff
records, PDF suppression and startup retry evidence.

Set SN-017 done locally with explicit scope, and SN-018 ready for the measured
reproducibility/performance baseline. Further native orchestration work must be
scoped separately if required by the next runnable product milestone; it is not
silently implied complete. Keep remote issues unchanged until publication is
authorized. No commit, release or remote synchronization is part of this decision.

Revisit acceptance if the declared profile, command permissions, engine inputs,
helper ownership or timing assumptions change. Broader application readiness
requires its own evidence; successful extraction does not establish it.
