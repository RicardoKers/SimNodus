# ADR 0101: Simulations run in a separate engine process

Date: 2026-10-19. Status: accepted (task SN-151, requirement SIM-08; direction
of [PRODUCT_ARCHITECTURE.md](../architecture/PRODUCT_ARCHITECTURE.md) and
[ADR 0078](0078-qt-widgets-worker-boundary.md)).

## Context

Until 0.4, ngspice and the STM32 emulator ran on a worker thread of the editor.
A crash in either (an access violation in ngspice, a bug in the young emulator,
R-108, R-110) closed the editor; the recovery file limited the loss to one
minute of work. SIM-08 asks that an engine crash cannot crash the editor or
lose the document.

## Decision

- **`simnodus-engine`** is a separate, Qt-free executable next to
  `simnodus.exe`. It runs the unchanged `SimulationSession` for the editor
  (`sim::EngineServer`). The editor starts it on the first run and keeps it for
  the next runs; after a crash, the next run starts a new one.
- **Transport:** the engine's standard input and output, carried by `QProcess`
  in the editor. Messages are length-prefixed binary frames
  (`sim/engine_protocol.hpp`) in the machine's byte order: both ends are the
  same build; a version number in the Start message rejects a mismatch. The
  engine moves descriptor 1 to standard error at start, so text printed by
  ngspice cannot corrupt the protocol; the editor reads standard error and
  keeps its last lines for the crash message. The engine ends when its input
  closes, so it never outlives the editor.
- **Mirrored state:** the editor's `sim::RemoteSession` implements the same
  `sim::Session` interface as the in-process session. Commands (start, pause,
  stop, speed, controls, reset, UART input) go to the engine; about every
  20 ms the engine sends an Update with the status, latest values, *new*
  samples of every acquired vector, board status and pins, new UART bytes, new
  pin changes and, once, the transient result. The editor keeps them in the
  same ring buffers, so the canvas and the Signal Analyzer read local data at
  their own rate, as before.
- **Start request:** the schematic travels as project JSON (the `.snx` format,
  blocks and copied models included), with the acquired vectors, current
  probes and board requests. The engine loads the component library from the
  editor's library folder and adds the schematic's blocks. The editor also
  checks the circuit itself, so diagnostics need no round trip.
- **Controls** belong to the editor: its value is the current one at once;
  the engine's value is used only as the starting one.
- **Failure:** when the engine process ends during a run, the run fails with
  "The simulation engine stopped unexpectedly (exit code ...)"; the editor and
  the document are untouched. A stop the engine does not answer within 5 s
  kills it.
- `SIMNODUS_ENGINE` names another engine executable (tests), or `inprocess`
  to run in the editor's process (debugging).

## Alternatives

- **Same executable with an `--engine` switch:** no second file to package,
  but the engine would load Qt for nothing, and the test executables (which are
  not `simnodus.exe`) could not start it.
- **Remote procedure calls for every query** (samples, pins, UART): simpler
  engine, but the analyzer asks dozens of questions per frame; each would wait
  for the other process.
- **Shared memory for the sample rings:** fastest, but platform code and
  synchronization across processes for a gain not needed: mirroring the
  heaviest measured case (40 vectors, exact edges at a 10 µs step, about
  17 MB/s) kept the speed of the in-process run.
- **Local socket** instead of the standard streams: a name to choose and
  clean up, no benefit for one parent and one child.

## Consequences

- Packages contain `simnodus-engine.exe`; `--check-engine` runs example 01
  in it, so packaging and the installer check verify it.
- A crash of ngspice or of the emulator costs only the run. Process isolation
  is crash containment, not a security sandbox.
- Firmware paths are resolved in the editor and sent absolute; the GDB server
  now listens from the engine process (still 127.0.0.1 only).
- Tools > Check Simulation Engine still loads ngspice in the editor's process.
