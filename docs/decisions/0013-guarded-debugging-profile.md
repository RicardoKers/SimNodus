# ADR 0013: Reject unsupported debugger commands at the transport

Date: 2026-09-07. Status: accepted for the bounded E-05 experiment;
complete debugging compatibility and production extraction remain pending.

2026-09-10 follow-up: [ADR 0014](0014-bounded-cooperative-debugging.md) accepts
a separate cooperative profile and closes the bounded experiment gate. The
default rejection policy below remains unchanged for the unmodified backend;
its historical evidence and earlier pending statements are preserved.

## Context

The actual CubeIDE and plain-GDB experiments agree: a debugger CPU interrupt
can leave Renode's global time advancing to the outstanding host-grant boundary.
A successful CPU stop reply therefore cannot establish a joint circuit/MCU
pause. The scripted launch settings alone also cannot prevent another command
from being sent through the debugger console.

## Decision

Use the owned [GDB transport gate](../../tests/experiments/debugging/gdb_guard.py)
for a restricted experiment profile. The host authorizes one 5 ms interval.
Only bounded fixture inspection, breakpoint changes and authorized forward
continue/step packets reach the existing source-audited Renode stub.

Unknown commands, arbitrary monitor commands, memory/register writes, reverse
execution, interrupt, and detach during a grant fail closed. Keep the upstream
connection until the host terminates Renode, then release the relay. Reject the
command before forwarding it; report failure and publish no new joint commit.
A lost debugger connection during a grant uses the same failure path.

Do not call this behavior pause support. Rejection terminates the experimental
session and requires a fresh session for recovery. The previous ungated probes
remain useful measurements of backend limitations, not a supported alternate
route for arbitrary commands.

## Evidence and limits

The [guard contract](../../tests/experiments/debugging/GUARDED_PROFILE.md) was
recorded before its first real run. The [E-05 report](../experiments/E-05-results.md)
records real rejection/recovery and actual guarded CubeIDE evidence separately
from parser/authorization unit tests. Transport permission does not itself
validate time: the existing host still checks stop reasons, PCs, stable
inspection, monotonic endpoints and ngspice checkpoint agreement.

This is not a sandbox against hostile local processes. Renode's internal
loopback ports remain reachable by another local process; the controlled
experiment must point GDB at the relay. No authentication or OS isolation is
claimed. The allowlist is specific to the owned firmware, memory map, one CPU,
selected GDB and selected IDE build. It is not a general-purpose RSP proxy.

Reset remains full session recreation, not a debugger-only MCU reset. On
2026-09-08, three guarded IDE rejection/recovery pairs validated reset, detach
and interrupt diagnostics in the Eclipse console document, followed by the
normal stop sequence and same-IDE backend recreation/detach/reconnect at zero.
The 15-case transport matrix passed again with the same guard hash. See the
[lifecycle evidence](../experiments/evidence/E-05-ide-lifecycle-summary.json).
Two further [fault/recovery pairs](../experiments/evidence/E-05-ide-fault-summary.json)
passed actual Renode process loss and a two-second missing-stop deadline in
guarded CubeIDE, with failed pending grants, console diagnostics, no final
commit and fresh lifecycle recovery. This approves only the bounded DSF/console
experiment. Mouse-driven UI behavior, arbitrary failures, network timeouts and
the complete joint-debugging contract remain unvalidated. SN-016 stays open.

## Revisit when

A real backend extension demonstrates a joint stop/cancel contract, or a bounded
lesson needs additional debugger operations. Change capabilities only with new
measured evidence; do not loosen the packet policy to conceal an unsupported
action or mark a CPU-only interrupt as a joint commit.
