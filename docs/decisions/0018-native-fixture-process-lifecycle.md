# ADR 0018: native fixture process lifecycle on Windows

Date: 2026-09-10. Status: accepted for the SN-017 extraction slice.

## Context

The fixture already uses native session contracts, cancellation commands and
result ingress, but Python still creates Renode and the analog worker directly.
Their process lifetime must be explicit during normal exit, fault teardown,
recreation and loss of the immediate process owner.

## Decision

Add an opt-in `--process-runner` supervisor for both Renode and the persistent
ngspice worker. It uses C++20/Win32 to create a suspended child, assign it to a
non-inherited Job Object with kill-on-close, publish its actual PID, then resume
it. Only explicit stdin/stdout/stderr handles are inherited by the child. The
job handle is not inherited, so supervisor death terminates its owned tree.

Preserve the fixture arguments, working directory, environment and stdio. Use
Windows argv quoting for empty strings, quotes, spaces and trailing backslashes.
The Python shim exposes the child PID for independent listener checks and keeps
the supervisor PID distinct. The host still prepares the trusted fixture and
launches the small supervisor; this is not a general project-file execution API.

Normal shutdown remains the backend's existing `quit` command. A stop file
requests native job termination. The supervisor waits for the root and ensures
all job processes have exited before publishing a terminal record. Root exit
code and forced termination are recorded separately. A dead supervisor cannot
publish a fabricated successful terminal record. Recovery always creates fresh
processes and fresh lifecycle files.

The host bounds launch publication by five seconds and retains existing backend
startup/teardown watchdogs. Native forced root wait and remaining-descendant
cleanup each have five-second limits. The shim can kill a stalled supervisor,
which closes its job. These are lifecycle bounds, not a new virtual-time pause
capability or a guarantee about arbitrary blocked operating-system operations.

## Alternatives

Retain direct Python `Popen` as the default differential control. Migrating all
fixture scheduling into this supervisor was deferred: the supervisor owns
process lifetime, not time, grants, GPIO/ADC exchange or debugger policy.
A process group without lifetime ownership would not cover supervisor death.

## Consequences

Both engines can now be created and reaped by native code, while the Python
harness retains configuration, endpoint verification, GDB relay and orchestration.
An unexpected supervisor exit is distinguished from orderly or forced child exit.
Job containment is resource ownership, not a security sandbox. Host-harness death
without supervisor death and hostile local interference are not newly approved.

The [protocol](../../tests/headless/PROCESS_LIFECYCLE.md) and
[evidence](../experiments/evidence/SN-017-process-lifecycle-summary.json) cover
real engine recreation, engine/owner failures, descendant cleanup, argv/stdio,
startup errors and fresh recovery. No engine, firmware, dependency or IDE startup
change is required. The PDF suppression fix remains unchanged.

## Revisit criteria

Revisit the supervisor/shim boundary when consolidating native orchestration.
Non-Windows process ownership, arbitrary child trees, host-harness death and
production packaging require their own evidence. ADRs 0014–0017 retain their
capability, deadline and failure restrictions.
