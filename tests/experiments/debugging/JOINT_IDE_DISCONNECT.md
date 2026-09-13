# Active debugger connection loss in the joint profile

Predeclare an abrupt GDB process-loss injection after observed CPU progress in
the paced interval following GPIO high. The Java runner must identify exactly
one descendant of its own IDE process whose executable matches the configured
GDB, then destroy only that process. No MI interrupt or orderly detach is sent.

Require the relay to detect connection loss while a CPU grant remains armed,
with no rejected command payload and Renode still alive. The coordinator must
cancel the CPU grant, validate remaining-time/sink accounting, and verify the
persistent circuit still matches its last accepted boundary. Terminate the
backends before announcing session failure. Publish neither a third joint
checkpoint nor a verified GDB pause notification. Require an actual IDE console
model diagnostic within two seconds of the process-loss request, plus accepted
IDE close, zero IDE exit and removed owned listeners.

Run three fresh disconnect/recovery pairs. Each recovery starts new engines and
completes the joint sequence with ADC code 3541. Run the default guarded lifecycle
regression after the shared Java/driver change. This is abort and fresh recovery,
not resumption of a partially executed disconnected session. The existing
unpaced interruption and other fault boundaries remain open.

## Socket-reset correction

The first candidate observed Windows socket reset (10054), not orderly EOF.
The inherited outer exception handler closed the upstream context before reporting
failure. Keep that failed candidate. The cooperative relay now catches socket
errors inside its connection context, records the native error, and retains the
Renode connection until explicit host close. A loopback SO_LINGER reset test
requires upstream recv to time out before host close and EOF only afterward.
The driver also permits the expected transport rejection while cancellation is
pending; a pass still requires complete failed-session acknowledgement and cleanup.
Default relay policy is unchanged.

## Verified result (2026-09-09)

Three fresh active-disconnect/recovery pairs passed with matching source/binary
hashes. The IDE forcibly terminated only its identified GDB descendant. No MI
interrupt was intercepted. The relay detected loss while the CPU grant was
active; cancellation returned unused time and matching sinks. Analog copied
state remained at its last accepted boundary. The host terminated both engines
before announcing failure and never published a third checkpoint or verified
GDB pause notification. Host handling took 121.2570, 167.6247 and 190.3233 ms;
the IDE independently enforced the two-second request-to-diagnostic limit.

Each fresh recovery completed the paced joint sequence and ADC code 3541. All
IDE sessions acknowledged closure, exited zero and removed owned listeners.
The default guarded lifecycle's nine stop/circuit records remained exact with
reset/reconnect at zero. All 13 relay tests passed, including the new loopback
socket-reset retention test. [Evidence](../../../docs/experiments/evidence/E-05-joint-ide-disconnect-summary.json)
preserves the failed first candidate and the corrected runs.

Run with a new output directory:

```powershell
python tests/experiments/debugging/cubeide.py --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --output build/sn016/joint-disconnect-next --profile joint --guarded --joint-fault disconnect --experimental-renode build/sn016/source-notification
```

Omit `--joint-fault disconnect` in a fresh directory for recovery. This validates
abort and a new session after active debugger loss; resuming the interrupted
session is unsupported. Unpaced interruption, other fault boundaries and the
existing Eclipse log exception remain open. SN-016 stays in progress.
