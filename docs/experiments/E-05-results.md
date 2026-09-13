# E-05 coordinated debugging: measured partial result

Updated: 2026-09-09. Task: SN-016. **The actual CubeIDE launch and bounded
breakpoint/step path work; the complete joint-debugging contract has not passed.**
SN-016 remains in progress. No production debug adapter is implemented.

## Scope and environment

The [fixed experiment contract](../../tests/experiments/debugging/README.md)
remains the acceptance baseline. Plain GDB completed three local repetitions
before this continuation (`build/sn016/plain-debug7/summary.json`). The new
executor launches STM32CubeIDE itself, which creates a generic GDB hardware
session through Eclipse DSF; it does not substitute an independently launched
GDB process for an IDE test.

The installed directory is named `STM32CubeIDE_2.1.1`, but the running product
reports **Version 2.2.0**. This evidence applies to that observed installation,
not an untested 2.1.1 release. Observed bundles:

| Bundle | Version |
|---|---|
| `org.eclipse.cdt.dsf.gdb` | `7.3.0.202512020204` |
| `org.eclipse.cdt.debug.gdbjtag.core` | `10.8.500.202512020204` |
| `org.eclipse.debug.core` | `3.23.200.v20251107-0507` |

Renode is the pinned 1.16.1 Windows portable build; ngspice is the pinned 47 DLL.
The CubeIDE-bundled Arm GDB reports `15.2.90.20241229`. All 14 pinned dependency
files and six Renode GDB audit sources passed verification. The owned ELF is
unchanged: SHA-256 `2a02d443a5d627b0f82c3a835ef385ba3e6f7a8607a829d539781cee4bdc9c5c`.
Its existing build record retains identical rebuild evidence and DWARF-4.

## Measured IDE actions

The host grants 5,000 us once. The debugger controls CPU execution within that
pending grant. Register and mailbox inspection checks virtual time before and
after access. The final session is terminated before the pending grant can
complete; that control command must fail and cannot publish another commit.

| Stop | Reason from DSF | PC | Observed time (ns) |
|---|---|---|---:|
| GPIO marker | BREAKPOINT | `0x080000f8` | 2010875 |
| One instruction | STEP | `0x080000fa` | 2011000 |
| GPIO after write | BREAKPOINT | `0x08000102` | 2011375 |
| Step target | BREAKPOINT | `0x08000120` | 2012125 |
| Source next 1 | STEP | `0x08000122` | 2012250 |
| Source next 2 | STEP | `0x08000126` | 2012500 |
| Source next 3 | STEP | `0x0800012a` | 2012875 |
| ADC marker | BREAKPOINT | `0x08000134` | 4010750 |
| ADC result stored | BREAKPOINT | `0x080001b8` | 4027000 |

The source next sequence crosses the marker helper without entering its frame.
The final mailbox is `[0x534e3035, 4, 1, 1, 2048, 1, 0, 0]`: four ticks, one GPIO
change, one ADC read, result 2048, one helper result, and no firmware fault.
The ADC receives the known direct value of 1,650,000 microvolts; the RC output is
not connected to the ADC in this experiment.

Three fresh normal IDE runs (`resume-normal-04`, `resume-normal-05`, and
`resume-normal-06`) produced identical stop reasons, PCs, mailbox bytes, virtual
times and circuit checkpoints. Their eight common marker/step stops and final
4,027,000 ns endpoint also match the earlier plain-GDB signature. The current
source additionally passed `resume-reset-03` and `resume-pause-02`; the latter
observed 8,000 ns before the 100 ms global endpoint and a 26,321,400 ns response.
The [compact evidence](evidence/E-05-partial-summary.json) retains hashes,
normalized launch attributes, results and raw-log fingerprints.

Each circuit checkpoint is a **fresh real ngspice replay from zero** using the
observed GPIO edge schedule. It is not a persistent solver pause/resume or
closed-loop electrical simulation. Nine final times match their debugger
checkpoints within the predeclared 1 ps floating-point tolerance. The final RC
output is approximately 2.860317557 V. Time regression, a stop outside the grant,
a changed expected PC/reason, or an unmatched circuit endpoint rejects the run.

The reset profile starts fresh Renode and ngspice state at zero and verifies an
empty firmware mailbox. It detaches the IDE, checks time through external
control, then reconnects through a fresh IDE debug launch with no host grant.
Both observations remain zero. This is evidence for recreation and stopped
reconnection, not a successful MCU-only reset inside a coupled session.

## Interrupt limitation

The IDE interrupt test repeats the plain-GDB finding: halting the CPU does not
halt Renode's global virtual-time grant. In `resume-pause-01`, the stop was
observed at 8,000 ns, the response took 24,084,300 wall-clock ns, and the global
time subsequently reached the authorized 100,000,000 ns boundary. The runner
records `committed=false` and `supported=false`. Only Renode participates in
this interrupt case; no analog state is fabricated at the CPU stop.

A passing interrupt *measurement* does not approve joint pause. Disconnect
inside an outstanding grant and debugger-only reset also remain unsupported.
The reset diagnostic in the probe is a scope declaration, not a production
command interceptor. The host does not yet mediate arbitrary user IDE commands.

## Reproduction and retained evidence

From the checkout, with the installed CubeIDE directory supplied explicitly:

```powershell
python tests/experiments/debugging/cubeide.py --ide C:/path/to/STM32CubeIDE --profile normal --output build/sn016/ide-normal-new
python tests/experiments/debugging/cubeide.py --ide C:/path/to/STM32CubeIDE --profile reset --output build/sn016/ide-reset-new
python tests/experiments/debugging/cubeide.py --ide C:/path/to/STM32CubeIDE --profile pause --output build/sn016/ide-pause-new
```

Each output directory must be new. The executor compiles the owned Java source
using the IDE's bundled JDK, assembles its experiment plugin, and creates a new
OSGi configuration and IDE workspace. It reads the installation without
modifying it. It requires the already prepared E-05 firmware/native helpers,
SN-019 loopback extension, and pinned backend files under `build/`.

Complete launch attributes are in
[CubeIdeRunner.java](../../tests/experiments/debugging/cubeide/src/org/simnodus/e05/CubeIdeRunner.java).
The launch uses the bundled GDB and loopback remote target, loads symbols, and
disables image download, reset, halt, initial resume, build, and custom startup
commands. It starts neither ST-LINK nor OpenOCD. Generated `.launch` files remain
in each workspace, together with the IDE console, executor output and failures.
The source frames are recorded through DSF; interactive editor source mapping
and manual toolbar workflows have not been validated.

Renode's GDB transport uses the owned, source-audited loopback bootstrap and
the official stub. It relies on the audited socket provider's private shape;
it is not a general public-API adapter or the originally proposed generated
stub variant. Both Renode listeners are checked for exact loopback addresses
and process ownership before attach, and their removal is checked after exit.
These checks concern Renode, not all services the installed IDE may start.

## Corrections made during this continuation

- Preserve the useful Java prototype as repository source under `tests/experiments/debugging/cubeide/`.
- Start the experiment bundle before constructing its DSF service tracker.
- After detach terminates a DSF launch, create another launch for reconnection.
- Read the all-stop reason from the DSF container rather than the thread's `CONTAINER` placeholder.
- Include the GPIO rise at its own circuit checkpoint and verify the output register.
- Check bounded native-helper completion before reading its output to EOF.
- Record source/executable hashes and validate structured stop/circuit evidence.

Earlier failed runs are retained and are not counted as passes. The runner's
`partial_probe_passed` status deliberately leaves `complete_e05_profile=false`.
Full E-05 approval still requires resolving or formally restricting the joint
pause/command-ownership contract and validating its enforcement. Do not infer
production-ready debugging or proceed on the assumption that IDE pause freezes
both engines.

## Restricted transport enforcement

[ADR 0013](../decisions/0013-guarded-debugging-profile.md) records a restricted
experimental policy; the original full joint-pause gate remains unpassed.
The [predeclared guard contract](../../tests/experiments/debugging/GUARDED_PROFILE.md)
is implemented in an owned Python relay. It buffers a complete RSP packet,
validates the checksum, and checks authorization before sending any of that
packet to Renode. It has a 16 KiB frame limit and a 2 second incomplete-frame
wall deadline. Invalid input causes session failure, not a fabricated stop reply.

Seven unit tests cover all packet split positions, coalesced acknowledgements,
corrupt/ambiguous checksums, size/deadline boundaries, prohibited commands,
host-grant authorization and fixture address ranges. These are protocol tests,
not substitutes for the real backend runs.

The real rejection matrix covers continue without a grant, monitor start,
monitor reset, memory write, interrupt, detach during an outstanding grant,
and loss of the GDB connection. Each case is followed by a fresh passing
breakpoint/instruction-step/real-ngspice recovery case. Rejections at idle also
verify unchanged backend time and mailbox; active-grant rejections require a
failed pending control command and no new commit. Both Renode listeners and
the relay listener must disappear after cleanup.

The guarded actual CubeIDE run in `build/sn016/guard-cubeide-final` passed all nine
normal stops, stable memory inspection, ADC result 2048 and matching ngspice
checkpoints, ending at 4,027,000 ns. The relay introduces no additional virtual-
time authority. Rejected commands are not forwarded, and the host destroys
Renode before closing the retained upstream debugger socket.

The allowlist was extended during failed development runs for the legacy
read-only thread query, the fixture's flash alias at zero, and the exact
`vCont;s:1;c` step form emitted by the IDE. The query is described in the
[official GDB protocol reference](https://sourceware.org/gdb/current/onlinedocs/gdb.html/General-Query-Packets.html).
No arbitrary monitor command, peripheral-register read, or memory write was
allowed. TCP_NODELAY is used on both relay connections. Failed development
runs remain outside the passing evidence.

```powershell
python tests/experiments/debugging/test_gdb_guard.py
python tests/experiments/debugging/guard_probe.py --output build/sn016/guard-new
python tests/experiments/debugging/cubeide.py --ide C:/path/to/STM32CubeIDE --profile normal --guarded --output build/sn016/guard-ide-new
```

The filtered endpoint is for the cooperating experiment. Another local process
could bypass it by connecting directly to Renode's private loopback ports.
The subsequent guarded IDE lifecycle and console-diagnostic evidence is recorded
below; a fail-closed session abort is not seamless IDE pause support.

The final matrix in `build/sn016/guard-matrix-final` passed all 15 real cases
(one normal run, seven rejections, and seven fresh recoveries). The final guarded
IDE run has identical stop and circuit arrays to `resume-normal-04`. Their
[compact evidence](evidence/E-05-guard-summary.json) includes input hashes,
raw-summary fingerprints, rejected-action diagnostics and listener cleanup.
These results use the same guard source hash. No generated runtime was deleted
and no remote publication was performed during this continuation.

## Guarded IDE lifecycle and diagnostics (2026-09-08)

Three actual CubeIDE rejection/recovery pairs passed with the same final source
hashes. Each rejected action was sent through the IDE's DSF command service.
The host verified the rejected RSP action before destroying Renode; the pending
5 ms grant exited with code 1, no final commit was published, and the diagnostic
was present in the real Eclipse `SimNodus experiment` console document.

| Rejected action | Rejection run | Fresh recovery run | Result |
|---|---|---|---|
| MCU-only monitor reset | `ide-reject-reset-03` | `lifecycle-05` | Passed |
| Detach during pending grant | `ide-reject-detach-01` | `lifecycle-06` | Passed |
| Interrupt | `ide-reject-interrupt-01` | `lifecycle-07` | Passed |

All paths above are under `build/sn016/`. Each recovery opens a fresh IDE
workspace and completes the nine normal stops through 4,027,000 ns. Stop and
circuit arrays are exactly equal to `resume-normal-04`. It then keeps that IDE
process open, removes the old Renode and relay listeners, and creates new
backend/relay instances with no inherited grant. The reset mailbox contains
32 zero bytes, a fresh ngspice replay reports time zero, and detach followed by
a new DSF launch reconnects at zero. Independent host time inspection after
detach also reports zero. Final backend and relay listeners are removed.

The parser/authorization suite passed all seven tests. The real transport matrix
was repeated as `guard-matrix-lifecycle`: all 15 cases passed with the same guard
hash as these six IDE runs. The additional allowlisted packet is only
`qP0000001f0000000000000001`, a legacy read-only query for the fixture's thread 1;
other mode/thread forms remain rejected. No execution or mutation permission
was added. See [compact lifecycle evidence](evidence/E-05-ide-lifecycle-summary.json)
for source/input hashes, raw-summary and transcript fingerprints, diagnostics,
cleanup checks, reset observations and the exact baseline comparison.

```powershell
python tests/experiments/debugging/cubeide.py --ide C:/path/to/STM32CubeIDE --profile normal --guarded --reject-action reset --output build/sn016/ide-reset-rejection-new
python tests/experiments/debugging/cubeide.py --ide C:/path/to/STM32CubeIDE --profile lifecycle --guarded --output build/sn016/ide-recovery-new
```

Repeat the pair for `detach` and `interrupt`, using new output paths. The owned
plugin explicitly requires `org.eclipse.text` to read the console document.
Host acknowledgement files are published atomically; Renode startup failures
terminate the process before propagating the error. Audit source copies belong
to each run directory.

Development failures are excluded from passing evidence: `lifecycle-01` exposed
the missing legacy query; `lifecycle-02` completed its debug markers but exceeded
the IDE exit deadline while startup dialog event loops were active. Restricted
execution attempts `sn016-lifecycle-03` and `sn016-lifecycle-04` in the visualization
workspace failed Renode root-directory access. `ide-reject-reset-02` rejected reset
correctly but failed console verification because `org.eclipse.text` was absent.
No security or analytics preference was changed to obtain the passing runs.

This validates the bounded DSF lifecycle and console model, not a mouse-driven
IDE workflow or persistence of the console after the test closes the IDE. The
circuit reset remains fresh known-schedule replay, not reset of a persistent
coupled analog session. The backend-loss and missing-stop timeout probes below extend this evidence;
they do not cover every possible backend failure or network timeout. Joint pause remains unsupported, the
original E-05 acceptance contract has not passed, and SN-016 remains open.

## Guarded IDE backend loss and stop deadline (2026-09-08)

Both newly implemented fault/recovery pairs passed in actual CubeIDE. They use
`--profile normal --guarded --fault backend` or `--fault timeout`, followed by a
fresh `--profile lifecycle --guarded` run. All four runs use the same executor
and Java sources. The transport policy is unchanged from the preceding matrix.

| Fault | Fault run | Recovery run | Observed result |
|---|---|---|---|
| Kill Renode during pending grant | `ide-fault-backend-01` | `ide-fault-backend-recovery-01` | Backend exit 1; relay detects connection loss |
| Unreachable stop deadline | `ide-fault-timeout-01` | `ide-fault-timeout-recovery-01` | Deadline observed after 2,007,066,100 wall-clock ns |

All paths are under `build/sn016/`. Backend loss is injected at the GPIO
breakpoint, inside the outstanding 5 ms host grant. The relay independently
reports Windows socket error 10054 with no rejected command payload. It is then
released only after the backend process has terminated.

For timeout, the IDE continues to the GPIO edge and holds PC `0x08000102`.
A wait for unreachable PC `0x0800fffc` expires after the declared two-second
wall deadline, below the five-second observation ceiling. Successful inspection
still reports the held PC and unchanged virtual time of 2,011,375 ns, while the
native grant remains pending. The host then terminates Renode normally. This
measures the experiment's missing-stop deadline, not an unresponsive MI command
or a network timeout, and introduces no virtual-time tolerance change.

Both cases verify the host diagnosis in the Eclipse console document, native
grant exit 1, absence of a final commit, successful IDE exit, and removal of
backend and relay listeners. Each recovery reproduces the exact nine baseline
stops and circuit arrays through 4,027,000 ns, then recreates the backend and
circuit replay at zero and detaches/reconnects without time advancement.
The [fault evidence](evidence/E-05-ide-fault-summary.json) preserves fingerprints,
observations and exact baseline comparisons. Seven guard tests and the
repository checks passed; these runs required no allowlist changes.

The bounded fault/recovery coverage planned in the previous continuation is
complete. The original joint-debugging gate remains open because rejection and
session destruction do not establish pause/cancel or persistent analog reset.
Mouse-driven IDE behavior, arbitrary failure modes and long-session robustness
are also outside this result. SN-016 is not marked complete and this result
does not approve production adapter extraction.

## Public Renode pause candidates (2026-09-08)

A separate [capability investigation](../../tests/experiments/debugging/PAUSE_CANDIDATES.md)
audited the exact time-source implementation and exercised two host-monitor APIs
at a held GPIO breakpoint. `PauseAndRequestEmulationPause false` and `PauseAll`
both returned within 100 ms, but neither completed the outstanding 5 ms grant.
Three observations per case and register/memory inspection remained at
2,010,875 ns. Each session was destroyed and followed by a passing full plain-GDB
recovery with real ngspice replay. See [source and execution evidence](evidence/E-05-pause-candidates-summary.json).

These observations cover cancellation of a held grant, not a running-CPU pause
or same-instance resume. They do not establish joint pause. The investigation
specifies a cancellable time-source extension and persistent-analog validation
as the next candidate, with no change to the guarded command policy. Its initial
harness failure is excluded from the recorded final evidence.

## Cooperative cancellation prototype (2026-09-08)

A separate experimental managed Renode build now cancels a held or running
single-CPU interval without destroying the backend. The unmodified source build
and patched build produced exactly equal full plain-GDB/ngspice normal results.
Both retain the pinned release's native CPU translators; managed sources, SDK,
resources and runtime provenance are recorded, not assumed binary-equivalent.

The [final three-repetition matrix](evidence/E-05-cooperative-cancellation-summary.json)
passed held-stop cancellation at 2,010,875 ns and 2,011,375 ns, repeated requests,
stable inspection, fresh authorization in the same process, running cancellation
at 8,011,375 ns, and completion precedence at 9,011,375 ns when cancellation
coincided with the endpoint. Global and registered sink times agreed in every
outcome. Source activation must be acknowledged before debugger continue;
the initially reversed ordering failed a repetition and was corrected.

This is backend evidence only. GDB still reports the target running after a
host-only pause, and a persistent analog domain was not part of the cancellation
matrix. Arbitrary concurrency, stuck sinks, multiple CPUs and tiny residual
grants remain unvalidated. The default guarded endpoint still rejects interrupt.
See [scope and measurements](../../tests/experiments/debugging/COOPERATIVE_CANCELLATION.md)
and [build reproduction](../../tests/experiments/debugging/SOURCE_BUILD.md).
SN-016 and the original joint-pause contract remain open.

## Verified GDB notification after cancellation (2026-09-08)

The [opt-in notification extension](../../tests/experiments/debugging/DEBUG_NOTIFICATION.md)
now verifies quiescent CPU/source time before invoking the normal Renode halted
event. Three fresh standalone-GDB repetitions passed: premature and wrong-time
requests were rejected, a verified running cancellation produced one SIGINT stop
at 8,011,375 ns, register/memory inspection remained stable, duplicates were
rejected, and new authorization plus GDB continue reached 9,011,375 ns in the
same backend. The original full GDB/ngspice case remained exactly equal to the
unmodified source-build reference. See [execution evidence](evidence/E-05-debug-notification-summary.json).

This extends the earlier backend-only prototype with measured GDB stop-state
handling. It does not validate actual guarded CubeIDE integration or persistent
analog joint pause. The pinned backend, default relay policy and SN-016's open
status remain unchanged.

## Cooperative pause in actual guarded CubeIDE (2026-09-08)

The [opt-in CubeIDE protocol](../../tests/experiments/debugging/COOPERATIVE_CUBEIDE.md)
passed three fresh IDE/backend repetitions. The relay retained each actual
interrupt, the host cancelled its pending 100 ms authorization, and the verified
backend notification produced a DSF SIGNAL stop. PC/mailbox inspection remained
stable. A separate active authorization followed by IDE continue advanced
exactly 5 ms. The pause times varied with wall-clock injection as predeclared;
there is no claim of deterministic asynchronous interrupt timing.

The default guarded lifecycle regression still exactly matches its nine-stop
and real-ngspice baseline, with reset/reconnect at zero. All 12 parser/state/relay
tests passed. [Source hashes and observations](evidence/E-05-cooperative-cubeide-summary.json)
are preserved separately from the default profile's evidence. Persistent analog
pause/resume and fault/reset/reconnect coverage of the new cooperative protocol
remain open. SN-016 is not complete and the default interrupt policy is unchanged.

## Persistent analog boundary candidate (2026-09-08)

A [single-circuit ngspice pause/resume probe](../../tests/experiments/debugging/PERSISTENT_ANALOG.md)
passed three identical repetitions at four known future boundaries, followed
by a 5 ms endpoint. `ngSpice_SetBkpt(T)` plus foreground `stop when time >= T`
met the fixed 1 ps tolerance; paused copied samples remained unchanged for
100 ms. Voltages agreed with the analytical RC result and uninterrupted
reference within the predeclared 10 microvolt tolerance. The final difference
from uninterrupted execution was about -7 pV.
[Evidence and source/binary hashes](evidence/E-05-persistent-analog-summary.json)
are preserved. This closes the isolated known-boundary analog candidate only.
Integrated persistent CPU/analog pause, GPIO/ADC exchange, and cooperative
fault/lifecycle coverage remain open; SN-016 remains in progress.

## Persistent joint checkpoints through GPIO and ADC (2026-09-08)

The [bounded joint probe](../../tests/experiments/debugging/JOINT_PERSISTENT.md)
now integrates the experimental CPU cancellation with one persistent RC circuit.
Three identical plain-GDB runs passed at 2010875, 2011375, 4010750 and 4027000 ns.
Each CPU grant was cancelled before analog advancement; a joint checkpoint was
recorded only after time agreement and stable inspection. The observed GPIO high
changed the persistent analog source. Its 2.8531143502144816 V ADC-boundary value
was supplied as 2853114 microvolts, and firmware stored the expected code 3541.
The sample was held through conversion as predeclared.

Killing the analog helper before the third acknowledgement failed the operation
with only two joint checkpoints and no ADC exchange. Cleanup removed Renode
listeners; three new normal runs recovered the full sequence.
[Evidence and matching hashes](evidence/E-05-joint-persistent-summary.json)
are retained. This validates the held-breakpoint path with persistent state;
asynchronous joint pause in the IDE and remaining cooperative fault/lifecycle
gates are still open. SN-016 remains in progress.

## Paced asynchronous joint pause in actual CubeIDE (2026-09-08)

The [joint IDE candidate](../../tests/experiments/debugging/JOINT_IDE.md) passed
three complete actual-IDE sessions. During the interrupt interval only, the
experimental bridge sleeps 1 ms per TimePassed callback; it does not change
virtual time. Interrupts stopped the CPU and persistent circuit at 2014000,
2015000 and 2012000 ns, with nonzero capacitor state. The coordinator cancelled
the CPU grant, settled the analog circuit, then emitted the verified backend
notification. DSF reported SIGNAL and stable inspection; resume preserved the
circuit through GPIO/ADC exchange and firmware code 3541. All three sessions
cleaned up normally. The default nine-stop lifecycle regression remained exact,
including reset/reconnect at zero, and all 12 transport tests passed.

[Evidence](evidence/E-05-joint-ide-summary.json) retains unpaced candidates that
failed to interrupt inside the GPIO/ADC interval, plus one session that completed
the simulation but failed Eclipse shutdown. This establishes paced scheduling
evidence only. Unpaced joint interruption, full cooperative fault/lifecycle
coverage, and the intermittent IDE shutdown issue remain open; SN-016 remains
in progress and the pinned default interrupt policy is unchanged.

## Disposable IDE startup/closure correction

The [shutdown investigation](../../tests/experiments/debugging/IDE_SHUTDOWN.md)
identified the actual Defender-suggestion preference node as `org.eclipse.ui`.
The first candidate used the wrong node and needed the user's confirmation;
it is explicitly excluded from unattended evidence. Each new configuration now
seeds the correct skip-suggestion preference and disables analytics for that
process. This changes neither Windows protection nor installation preferences.

Three fresh paced-joint sessions and a guarded lifecycle regression then closed
automatically, acknowledged workbench close, exited zero and removed listeners.
The default nine-stop/circuit results remained exact. The Eclipse
PerspectiveManager exception still appears in logs and is not claimed fixed.
[Detailed evidence](evidence/E-05-ide-shutdown-summary.json) separates that issue
from the verified disposable-session closure. The full SN-016 gate remains open.

## Analog loss before joint IDE pause acknowledgement

Three [actual-IDE failure/recovery pairs](../../tests/experiments/debugging/JOINT_IDE_FAILURES.md)
passed with the same source and binary hashes. After CPU cancellation, the host
killed the analog helper and attempted advancement. Loss ended the session
before any third joint checkpoint or verified GDB pause notification. The IDE
console showed the failure within the fixed deadline. All recoveries restored
persistent GPIO/RC/ADC operation and firmware code 3541; all IDE sessions closed
automatically and removed owned listeners. The default lifecycle regression
remained exactly equal to its nine-stop/circuit baseline with reset/reconnect.

[Evidence](evidence/E-05-joint-ide-loss-summary.json) records host failure handling
at 131.3522, 203.1762 and 139.4605 ms. This closes the single paced analog-loss
case only. Joint timeout/reset/reconnect and unpaced interrupt coverage remain
open, as does the existing Eclipse internal log exception.

## Joint IDE live-worker response timeout (2026-09-09)

Three [timeout/recovery pairs](../../tests/experiments/debugging/JOINT_IDE_FAILURES.md)
passed with the real analog helper alive but withholding acknowledgement.
The existing two-second response deadline expired, and failed-session handling
finished in 2.128-2.154 seconds, within the separately predeclared five-second
failure bound. The IDE displayed the diagnostic without publishing a third joint
checkpoint or verified GDB pause notification. Fresh recoveries restored ADC
code 3541 and closed automatically. The existing process-loss test and default
guarded lifecycle regression also passed; all nine baseline stop/circuit records
remained exact.

[Evidence](evidence/E-05-joint-ide-timeout-summary.json) records the distinction
between a live unresponsive worker and an exited process. This is not a solver
nonconvergence test. Remaining joint reset/reconnect and unpaced interruption
gates keep SN-016 open.

## Joint session recreation and reconnect (2026-09-09)

Three [joint lifecycle repetitions](../../tests/experiments/debugging/JOINT_IDE_LIFECYCLE.md)
passed in actual CubeIDE sessions. Each completed a paced joint sequence,
recreated both engines, detached/reattached DSF at zero, then completed the
sequence again. CPU time and firmware mailbox reset to zero, and the new
persistent circuit had zero time, voltage and samples throughout disconnect.
All six sequences reached ADC code 3541 and the same known firmware boundaries.
All sessions closed automatically and removed owned resources. The default
lifecycle's nine stop/circuit records remained exact.

[Evidence](evidence/E-05-joint-ide-lifecycle-summary.json) validates reset by
session recreation and reconnect at zero without a grant. It does not validate
in-place reset/rollback or a running disconnected session. Unpaced interruption
and remaining fault boundaries still keep the full E-05 gate open.

## Active debugger loss in the joint IDE profile (2026-09-09)

The [active-disconnect probe](../../tests/experiments/debugging/JOINT_IDE_DISCONNECT.md)
exposed a socket-reset path that left the upstream connection context before
host teardown. The cooperative relay now retains that connection while the host
cancels and closes Renode; a loopback reset test verifies the ordering.
Three actual-IDE process-loss/recovery pairs then passed without an MI interrupt
or false joint pause acknowledgement. Cancellation retained correct remaining-
time accounting, the analog state stayed at its last accepted boundary, and
fresh sessions restored ADC code 3541. IDE closure and listener cleanup passed.

[Evidence](evidence/E-05-joint-ide-disconnect-summary.json) includes the initial
failure and host handling times of 121.2570, 167.6247 and 190.3233 ms. All 13 relay
tests passed, and the default lifecycle regression remained exact. This supports
abort/new-session recovery after active debugger loss, not continuation of the
partially executed disconnected session. Unpaced interruption and other fault
boundaries remain open.

## Active CPU backend loss in the joint IDE profile (2026-09-09)

Three [CPU-loss/recovery pairs](../../tests/experiments/debugging/JOINT_IDE_BACKEND_LOSS.md)
passed after real Renode termination during a paced active grant. No interrupt
was requested, no cancellation outcome was invented, and the analog state stayed
at the last acknowledged boundary of 2011375 ns. The relay detected transport
loss and the host terminated the remaining worker before publishing the actual
IDE diagnostic. No third joint checkpoint or verified pause notification appeared.

Host handling took 70.1421, 69.6060 and 73.2565 ms, with the IDE independently
checking its two-second deadline. All three new sessions recovered ADC code 3541;
IDE closure, listener cleanup and the exact default lifecycle regression passed.
[Evidence](evidence/E-05-joint-ide-backend-loss-summary.json) retains matching hashes
and raw run paths. Recovery creates a new session; the lost CPU grant has no
known final state and cannot be resumed. The full E-05 gate remains open.

## Unpaced joint IDE characterization (2026-09-09)

Three fresh sessions disabled the bridge sleep callback without changing the
firmware, grant, strict pause interval or numerical tolerances. One passed at
3838000 ns with stable joint inspection and recovered ADC code 3541. Two reached
the ADC breakpoint at 4010750 ns before cancellation acknowledgement. Their
candidate outcomes were recorded and rejected before a third joint checkpoint
or verified pause notification. Owned relay/backend listeners were removed.
These are failed pause attempts; they do not validate orderly failure diagnosis
or normal IDE closure. The paced control and exact default lifecycle regression
passed with the instrumented driver.

[Evidence](evidence/E-05-joint-ide-unpaced-summary.json) retains all three runs.
The [characterization contract](../../tests/experiments/debugging/JOINT_IDE_UNPACED.md)
records observed progress and host timing, plus the next bounded investigation:
explicit arbitration between a requested pause and an already-hit breakpoint.
Unpaced interruption remains unapproved; do not convert these failures into
passes by relaxing the strict interval or claiming a fabricated SIGNAL event.

## Breakpoint before the joint pause request (2026-09-09)

Three fresh [ordered-case sessions](../../tests/experiments/debugging/JOINT_IDE_BREAKPOINT_FIRST.md)
passed without wall pacing. The IDE first observed the actual ADC breakpoint at
4010750 ns, then requested a joint pause from the host. Cancellation preserved
remaining-time/sink accounting; the persistent circuit reached that endpoint
and its ADC voltage was transferred once. The IDE retained BREAKPOINT, issued
no raw MI interrupt and received no fabricated SIGINT notification. After stable
inspection, a new grant reached the committed result with ADC code 3541.

Host acknowledgement took 129.6832, 127.8487 and 121.3612 ms; each IDE also checked
the two-second deadline. All owned processes closed successfully and listeners
were removed. The paced pause-first control and exact default lifecycle
regression passed. [Evidence](evidence/E-05-joint-ide-breakpoint-first-summary.json)
records all three matching-hash runs. This is breakpoint-before-request
ordering only; a raw interrupt already in flight at breakpoint arrival and
fault teardown in this new branch still require validation. Earlier strict
unpaced failures remain failed and the full E-05 gate remains open.

## Actual interrupt/breakpoint arbitration with host latency (2026-09-09)

Three [arbitration sessions](../../tests/experiments/debugging/JOINT_IDE_RACE.md)
passed with an actual protected MI interrupt, a 50 ms host cancellation delay,
and no CPU wall pacing. After interception, read-only samples still observed
CPU time before ADC. Cancellation then ended at 4010750 ns, and the IDE verified
BREAKPOINT/PC/time before the persistent circuit advanced. ADC voltage was
transferred once, inspection remained stable, and resumed firmware produced
3541. No fabricated SIGINT was issued. Host acknowledgement took 263.6071,
265.4288 and 262.9690 ms. IDE/worker closure and listener cleanup passed; the
paced SIGNAL and exact default lifecycle controls also passed.

Natural runs 01-05 took the SIGNAL path successfully. Natural run 06 exposed an
analog boundary failure at 3140000 ns and published no third checkpoint. Three
direct worker runs reproduced that failure without the IDE. The issue remains
unresolved; do not claim general unpaced support. [Evidence](evidence/E-05-joint-ide-race-summary.json)
retains all outcomes and source/binary hashes. Next inspect the ngspice
integration breakpoint versus foreground stop condition at the reproducible
boundary, preserving the 1 ps acceptance tolerance. The arbitration branch's
fault teardown and complete E-05 gate also remain open.

## Persistent analog boundary correction (2026-09-09)

Instrumentation of the reproduced 3140000 ns failure found an exact integration
sample at the requested double, followed by a foreground stop 12.5 ns late.
The pinned ngspice source uses raw >= comparison but three-ULP equality; decimal
parsing can produce a slightly higher threshold. The joint worker now uses that
equality condition while retaining its independent 1 ps endpoint check. It does
not round, relabel or accept an overshot state.

All 72 worker regression cases passed, covering 70 distinct intermediate times
and three occurrences of the original failing boundary. Maximum time error was
8.673617379884035e-19 s; RC accuracy, stable inspection and ADC voltage checks
passed. Three unpaced IDE arbitration runs plus paced pause, breakpoint-first
and delayed-race controls also passed with code 3541, clean process exits and
listener removal. [Evidence](evidence/E-05-analog-boundary-fix-summary.json) and the
[correction protocol](../../tests/experiments/debugging/ANALOG_BOUNDARY_FIX.md)
retain the before/after observations and matching worker hashes. This resolves
the reproduced joint-worker defect; fault teardown in arbitration and the full
E-05 gate remain open.

## Analog loss during interrupt/breakpoint arbitration (2026-09-09)

Three [fault/recovery pairs](../../tests/experiments/debugging/JOINT_IDE_RACE_LOSS.md)
passed after real analog-worker termination at the pending joint boundary. The
CPU had acknowledged cancellation at 4010750 ns and the IDE confirmed the
breakpoint, but the analog state remained at its accepted 2011375 ns boundary.
No ADC transfer, third joint checkpoint or verified SIGINT was published. Host
teardown preceded the IDE failure diagnostic; handling took 236.5169, 388.6805
and 283.8699 ms within the independently checked two-second IDE deadline.

Fresh sessions recovered ADC code 3541 and all IDE/process/listener cleanup
passed. The exact default lifecycle regression also passed. The first recovery
completed via SIGNAL instead of exercising the required collision and remains
a failed injection. The refined injector waits at least 50 ms and then observes
the actual breakpoint through read-only sampling before cancelling; its total
deadline is unchanged. [Evidence](evidence/E-05-race-analog-loss-summary.json)
retains the failed predecessor and three matching-source pairs. Timeout and
CPU/debugger loss in this arbitration branch remain open; SN-016 is in progress.

## Missing analog response during arbitration (2026-09-09)

Three [timeout/recovery pairs](../../tests/experiments/debugging/JOINT_IDE_RACE_TIMEOUT.md)
passed after the real analog worker deliberately stopped responding while still
alive. Actual MI interception and IDE breakpoint confirmation preceded the
missing response. The two-second acknowledgement deadline expired with the
worker alive; teardown then preceded the IDE failure diagnostic. No third
checkpoint, ADC transfer or verified SIGINT was published.

Response waits measured 2.0022431, 2.0057858 and 2.0068512 seconds. Total host
handling took 2.2628851, 2.2406752 and 2.2737371 seconds, within the pre-existing
five-second timeout-case limit also checked by the IDE. Fresh sessions recovered
ADC code 3541, with accepted closure and listener cleanup. The arbitration
process-loss and exact default lifecycle regressions also passed.
[Evidence](evidence/E-05-race-timeout-summary.json) preserves all runs and
matching hashes. This verifies missing response from a live process, not solver
nonconvergence. CPU/debugger loss in arbitration and full E-05 approval remain open.

## CPU backend loss before arbitration acknowledgement (2026-09-09)

Three [CPU-loss/recovery pairs](../../tests/experiments/debugging/JOINT_IDE_RACE_CPU_LOSS.md)
passed after actual IDE observation of the ADC breakpoint but before the host
cancelled or acknowledged the outstanding grant. Renode terminated nonzero,
the relay detected transport loss, and the third grant had neither a result
file nor an acknowledged outcome. Analog state stayed at the previously
accepted 2011375 ns point. No ADC transfer, third joint checkpoint or verified
SIGINT notification was published.

Host handling took 183.0872, 149.8341 and 182.0160 ms within the two-second IDE
request limit. Each fresh recovery completed arbitration and stored ADC code
3541; IDE closure and listener cleanup passed. The earlier joint CPU-loss case
and exact default lifecycle regressions also passed. [Evidence](evidence/E-05-race-cpu-loss-summary.json)
records matching source/binary hashes and all runs. The observed CPU breakpoint
did not become a fabricated host acknowledgement. Debugger loss in arbitration
and complete E-05 approval remain open.

## Debugger loss during arbitration and gate review (2026-09-09)

Three [debugger-loss/recovery pairs](../../tests/experiments/debugging/JOINT_IDE_RACE_DISCONNECT.md)
passed. After actual breakpoint confirmation, the owned GDB process was killed
while Renode remained alive with a grant armed and interruption pending. The
host cancelled at 4010750 ns, checked unused time/sinks, and preserved the
accepted analog boundary at 2011375 ns before teardown. No third joint
checkpoint, ADC transfer or verified SIGINT was published. Host handling took
308.3717, 291.6004 and 339.9856 ms within the two-second IDE deadline. Fresh
sessions recovered ADC code 3541; closure and cleanup passed. The prior
disconnect profile and exact default lifecycle regressions also passed.
[Evidence](evidence/E-05-race-disconnect-summary.json) retains all runs and hashes.

The [gate review](E-05-gate-review.md) now consolidates coverage and concrete
remaining work. Instruction-step/step-over evidence still comes from the default
replay profile, not the persistent joint sequence. Add these actions to the
persistent sequence and repeat combined lifecycle on one final version before
making a bounded capability/extraction decision. Full E-05 approval remains open.

## Persistent steps and combined IDE lifecycle (2026-09-09)

Three actual IDE lifecycle runs completed six persistent ten-checkpoint
sequences. Each sequence used a real instruction-step and three source-step-over
commands, with expected PCs, STEP reasons and increasing source lines while
remaining in step_target. The same circuit followed each acknowledged CPU
boundary, including nonzero RC state during source stepping. Asynchronous SIGNAL
pause, ADC transfer and resume retained the expected semantics and code 3541.
PC, r0/sp/lr, mailbox, time and analog snapshots stayed stable during inspection.

Both engines were recreated between the two sequences in each IDE, and actual
DSF detach/reattach preserved zero CPU/mailbox/analog state. All source/binary
hashes matched; closure and listener cleanup passed. Pause handling ranged from
45.8441 to 86.6593 ms within the independently checked two-second IDE limit.
The earlier joint and exact default lifecycle controls passed. The first
lifecycle candidate failed because the register reader assumed hexadecimal
format for decimal r0; its record is preserved after correcting the test to
compare actual GDB representations. [Evidence](evidence/E-05-joint-steps-summary.json)
and the [protocol](../../tests/experiments/debugging/JOINT_IDE_STEPS.md) record
all outcomes. The [gate review](E-05-gate-review.md) now marks persistent IDE
step/lifecycle integration measured; plain-GDB reconciliation and extended-profile
fault/race integration still precede the final bounded capability decision.


### 2026-09-09: plain GDB persistent fixed-step reconciliation

Three fresh direct-GDB runs passed nine persistent checkpoints, including one
instruction step, three source step-over commands, stable r0/sp/lr inspection and
ADC code 3541. All fixed times and GPIO states match the six extended IDE
sequences; 162 analog comparisons differ by at most 3.5084e-11 V. Relevant engine,
worker, GDB and firmware binaries match. Three original four-stop regression
runs also passed. Evidence: `E-05-plain-joint-steps-summary.json`; protocol:
`tests/experiments/debugging/PLAIN_JOINT_STEPS.md`. SN-016 remains in progress:
direct-GDB asynchronous pause/reset integration and extended fault/arbitration
coverage remain pending. This does not change milestone dates or open SN-017.

### 2026-09-09: direct-GDB persistent lifecycle cycle closed

Three lifecycle repetitions passed, each with two identical nine-checkpoint
persistent step sequences (54 checkpoints total). The first session exits
normally before both engines are recreated. Actual MI detach and reconnect use
the same GDB process in the recreated session; CPU/mailbox/time and untouched
analog state remain zero and stable. All GDB/backend exits were zero, old
listeners were removed and final ADC remained 3541. Three original four-stop
regressions also passed. Evidence: `E-05-plain-joint-lifecycle-summary.json`.
The completed cycle is the direct-GDB fixed-step recreation/reconnect extension;
SN-016/E-05 remains open for asynchronous direct-GDB pause integration, extended
fault/arbitration integration and the bounded capability decision. The user
requested a conversation handoff at this boundary; see `NEXT_CONVERSATION.md`.

### 2026-09-09: direct-GDB asynchronous host pause

Three lifecycle repetitions passed six extended sequences with 54 fixed
checkpoints and six host-requested asynchronous pauses. CPU cancellation and
analog endpoint checks preceded verified SIGINT notification. Pause endpoints
were 2013000 or 2014000 ns, with 42.7100 to 54.3480 ms acknowledgement. Stable
inspection, same-GDB recreated reconnect, ADC 3541 and normal cleanup passed.
The 324 comparisons with prior IDE fixed boundaries had maximum analog difference
1.7969e-11 V; three four-stop controls matched the prior control exactly.
[Evidence](evidence/E-05-plain-joint-pause-summary.json) and the
[protocol](../../tests/experiments/debugging/PLAIN_JOINT_PAUSE.md) retain details.
This uses paced host cancellation, not a raw MI interrupt or guarded relay.
Extended IDE fault/arbitration integration and the capability decision remain
pending; SN-016 is not complete and SN-017 has not started.

### 2026-09-10: extended IDE fault/arbitration matrix

All 35 sessions passed in `build/sn016/steps-fault-matrix-05`. The matrix contains
three initially paced arbitration fault/recovery pairs for each of analog loss,
live-worker response timeout, CPU loss and debugger loss; one continuously paced
extended pair per fault; and extended lifecycle, shorter arbitration and exact
default lifecycle controls. Every fault preserved seven joint checkpoints and
the last analog state at 2012875 ns, with no ADC transfer or joint pause
confirmation. CPU loss retained seven CPU acknowledgements; debugger loss
cancelled the eighth CPU grant without publishing an eighth joint checkpoint.
Arbitration recoveries preserved BREAKPOINT and transferred ADC once; all fresh
recoveries reached firmware code 3541. IDE closure and listener cleanup passed.

Three earlier unpaced injection failures remain failed. Initial pacing is now
explicitly released only after actual retained MI interruption and pre-ADC
observation; the remaining delay/breakpoint phase runs without callback sleep.
One later recovery encountered transient PermissionError reading a CPU result.
It remains failed. The reader now retries under its original deadline without
acknowledging unreadable state; all 14 unit tests passed, including this case.
The final real matrix needed no natural read retry. No deadline or numerical
tolerance was relaxed, and no file permissions or Defender settings changed.

See the [protocol](../../tests/experiments/debugging/JOINT_STEPS_FAULTS.md) and
[evidence](evidence/E-05-steps-fault-matrix-summary.json) for matching hashes,
per-run observations and all predecessors. This closes bounded extended-profile
integration with initial pacing, not general unpaced behavior or full E-05
approval. The final capability decision/gate review remains before SN-017.

After the shared bridge change, direct GDB also passed three recreated lifecycle
repetitions (six full step/pause sequences) with matching shared hashes, stable
inspection, ADC 3541 and normal cleanup; see
[regression evidence](evidence/E-05-steps-fault-plain-regression-summary.json).

### 2026-09-10: final bounded gate and release-note startup fix

The [final guarded direct-GDB check](../../tests/experiments/debugging/FINAL_GATE.md)
passed six recreated step/pause sequences with actual MI interrupts retained by
the cooperative relay. CPU cancellation and persistent analog agreement preceded
SIGINT, with 41.5932–91.5682 ms request-to-stop time and final ADC 3541. Shared
guard, coordinator, bridge and engine hashes match the fault matrix. Three
original four-stop regressions also matched their reference exactly.

Disposable IDE configurations now seed the active documentation plugin's
major.minor.micro preference as already seen. The installed ReleaseNotesOpener
uses this ConfigurationScope key to avoid opening DM00603738.pdf on startup.
The active key is 2.3.500; the startup executor verifies it. Three actual IDE
controls passed, including two extended lifecycle runs and exact default
lifecycle. Existing PDF windows and the installed IDE were not changed.

[Final evidence](evidence/E-05-final-gate-summary.json) and the
[case-by-case review](E-05-gate-review.md) support
[ADR 0014](../decisions/0014-bounded-cooperative-debugging.md): SN-016 is done
for the declared bounded cooperative profile, and SN-017 is ready. Raw partial
flags and unsuccessful predecessors remain unchanged. General unpaced behavior,
arbitrary workloads and production readiness are not approved by this gate.
