# E-05 coordinated debugging experiment

This directory owns SN-016's bounded Windows experiment. It tests plain GDB
first and then STM32CubeIDE against the real pinned Renode process, owned
STM32F103C8 firmware, and the real pinned ngspice DLL. It is not a production
debug adapter or a claim that arbitrary IDE actions preserve co-simulation
state.

## Audited transport constraint

Renode 1.16.1.19220 creates its standard GDB transport with
`SocketServerProvider.Start`. At the pinned infrastructure revision, that method
binds `IPAddress.Any`. E-05 must not open that listener.

The experiment instead generates a source-pinned GDB stub variant whose socket
provider binds only `IPAddress.Loopback`. The transformation must preserve the
GDB packet and CPU-control behavior, record input and generated hashes, and
retain the upstream MIT notice. Before connecting a debugger, the runner must
verify that the GDB and external-control ports are owned by the Renode process
and listen only on `127.0.0.1`. It must verify port cleanup after shutdown.

## Predeclared ownership and consistency profile

- The experiment host is the only authority that grants a bounded simulation
  interval. GDB may select CPU run or halt state inside that grant; it may not
  start the emulation or grant an independent interval.
- GDB-server autostart, `monitor start`, reverse execution, and background
  free-running are outside the supported profile. The runner must reject or
  avoid them explicitly.
- A debugger stop is an observation, not a joint commit. The host reads Renode's
  actual stop time and program counter, prevents another Renode grant, advances
  ngspice forward to that time when the circuit participates, and commits only
  after both domains report the same boundary.
- Renode must not pass the requested host boundary. ngspice must not pass the
  observed debugger stop. Any overshoot, time regression, missing stop reply, or
  state change during inspection invalidates the action and the process.
- Register and memory inspection must leave virtual time unchanged. Instruction
  step and source step-over may advance Renode; their observed endpoint becomes
  the proposed boundary, and any participating circuit must follow before
  commit.
- ADC input uses E-04's direct, known-schedule integer-microvolt path. The ADC
  case does not claim electrical acquisition or ngspice participation.
- Reset is a coordinated session operation accepted only while stopped. It
  recreates or resets every active backend and verifies time and fixture state.
  A debugger-only reset is unsupported and must be rejected with a diagnostic.
- Disconnect never transfers time ownership to Renode. Continued execution
  requires a fresh debugger connection and an explicit host grant.
- Wall-clock deadlines detect communication failure and never advance virtual
  time. After timeout, malformed state, or backend termination, no new joint
  state is committed; recovery uses fresh backend processes.

## Predeclared cases and gates

Run the complete plain-GDB profile in three fresh process groups. Build the ELF
twice and require identical bytes, retained debug information, and stable marker
symbol addresses. Repeated discrete stop reasons, program counters, firmware
observations, and committed virtual times must match exactly.

1. Start both engines paused at time zero, connect the bundled Arm GDB, and
   require no virtual-time change during attach and inspection.
2. Set a source breakpoint on `gpio_change_marker` inside a larger host-requested
   interval. Require a breakpoint stop before that interval's end, no Renode
   overshoot, no ngspice overshoot, and a joint commit at the observed stop.
3. Inspect PC, registers, source location, and firmware memory while stopped.
   Require identical times before and after inspection.
4. Execute one instruction step. Require one completed instruction, a changed PC,
   a nondecreasing observed time, and circuit catch-up before commit if time
   advances.
5. Execute source step-over across the owned marker helper. Require a later
   source location without entering the helper, a nondecreasing observed time,
   and the same joint-commit rule.
6. Continue under a bounded host grant to `adc_read_marker` after supplying the
   declared direct voltage. Require the expected ADC result, marker address,
   sample semantics, and actual stop time.
7. While a bounded grant is active, issue GDB interrupt and require a stop reply
   within 2 seconds. Record the actual stop state; do not fabricate advancement
   from the wall-clock deadline.
8. Disconnect while stopped. Require connection cleanup, no hidden time
   advancement, and no independent continuation. Reconnect explicitly before
   any later grant.
9. Perform a coordinated session reset and require Renode, ngspice, firmware
   mailbox, and committed time to return to their declared initial state. Reject
   debugger-only reset as unsupported.
10. Terminate Renode during a pending command and separately expire an
    unreachable-stop deadline. Each action must fail without commit, release its
    listeners, and be followed by a complete passing case in fresh processes.

After plain GDB passes, run the selected STM32CubeIDE 2.1.1 generic GDB hardware
debug launch against the same loopback server and ELF. The recipe must use the
CubeIDE-bundled GDB, must not launch ST-LINK/OpenOCD or download firmware, and
must repeat attach, GPIO breakpoint, inspect, instruction-step, step-over,
continue to ADC read, interrupt, disconnect, and session reset. Record the
actual CubeIDE and plugin versions and the complete portable launch settings.

CubeIDE compatibility passes only if the IDE actually launches the session and
the backend evidence satisfies the same ownership and consistency gates. Merely
validating a launch file or reusing the bundled GDB outside CubeIDE is not an IDE
pass. If CubeIDE automation cannot be completed, report the plain-GDB result and
the IDE limitation separately; do not mark E-05 complete.

The committed report must retain exact backend, source, executable, extension,
firmware, circuit, GDB, CubeIDE, and plugin hashes or versions; commands and
launch settings; requested, observed, and committed times; stop reasons and
addresses; process/listener supervision; failures; recovery; and unsupported
operations. These gates must not be relaxed after observing results.

## Reference sources

- [Pinned Renode GDB stub](https://github.com/renode/renode-infrastructure/blob/add012af003a0f620d3da52828262676f374d121/src/Emulator/Main/Utilities/GDB/GdbStub.cs)
- [Pinned Renode socket provider](https://github.com/renode/renode-infrastructure/blob/add012af003a0f620d3da52828262676f374d121/src/Emulator/Main/Utilities/SocketServerProvider.cs)
- [Renode GDB documentation](https://renode.readthedocs.io/en/latest/debugging/gdb.html)

The source files above are audit inputs. Generated experiment code remains in
ignored build output and retains its upstream license notice.
