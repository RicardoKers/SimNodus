# E-05 guarded transport experiment contract

Status: pre-execution contract, 2026-09-07. This adds a restricted experiment;
it does not replace the original E-05 acceptance criteria or approve joint pause.

The host places an owned, bounded GDB RSP filter between the debugger and the
existing audited Renode loopback listener. Both remain local. This is command
arbitration for a cooperative experiment, not a sandbox against another local
process that deliberately connects to the private backend ports.

Only checked read/attach queries, owned-fixture breakpoint changes, and host-
authorized continue/step packets may reach Renode. Execution permission must be
armed before the host starts its single pending RunFor grant. It remains bound
to that session and cannot be reused for another interval. Arbitrary monitor
commands, register/memory writes, image loading, reverse execution, interrupt,
and detach while a grant is pending are unsupported. Only the exact elapsed-
virtual-time monitor query is allowed. Unknown commands fail closed.

On an unsupported command, corrupt checksum, oversized packet, or debugger
connection loss during a grant, retain the upstream connection until the host
terminates Renode. Do not forward the command, inject a fictitious stop reply,
or release the pending grant by prematurely detaching. Discard tentative state.
The host then closes the transport and verifies listener cleanup. Recovery uses
fresh backend processes. There is no pause or rollback claim.

Predeclared checks:

1. Fragmented packets are withheld until their complete checksum is valid;
   multiple packets in one read remain distinct. Reject corrupt, escaped,
   oversized and unterminated frames within a bounded deadline.
2. Continue without a host authorization, monitor start/reset, register/memory
   mutation, interrupt and detach during a grant never reach the backend.
3. Against real Renode, inspect time/memory, reach a GPIO breakpoint inside a
   5 ms grant, step one instruction, reconstruct the matching real ngspice
   checkpoint, and retain unchanged inspection time.
4. For each real rejection, retain packet diagnostics, terminate Renode, require
   a failed pending grant when present, verify all listeners disappear, then
   run the passing normal case in a fresh process group.
5. Route an actual CubeIDE normal session through the same filter and require
   its existing exact stop, mailbox and circuit evidence. Any required extension
   to the allowlist must be explicit and recorded; never allow arbitrary monitor
   access to get the IDE to connect.

The initial byte-frame limit is 16 KiB and incomplete-frame wall deadline is
2 seconds. These are experiment limits. They never grant virtual time.

## Guarded IDE fault extension (2026-09-08)

The `--fault backend` probe kills the actual Renode process while the IDE holds
the GPIO stop inside a pending 5 ms grant. It requires a nonzero backend exit
and independently detected relay connection loss before releasing the relay.
The `--fault timeout` probe continues to the GPIO edge, then waits for the
unreachable PC `0x0800fffc` while that stop remains held. Its stop deadline is
2 wall-clock seconds with an upper observation bound of 5 seconds; successful
inspection must still report PC `0x08000102`, unchanged virtual time and a
pending grant. This is a missing-stop deadline, not a network timeout.

Both probes require a failed native grant, no final commit, a verified Eclipse
console diagnostic, removed listeners and a fresh passing guarded lifecycle
run afterward. Neither can be combined with command-rejection injection.
The IDE startup deadline is separate from the stop deadline. An unrelated
command rejection, failed inspection, unexpected exit or late deadline is a
failed probe, not evidence for the intended fault.
