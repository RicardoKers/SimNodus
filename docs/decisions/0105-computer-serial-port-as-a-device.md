# ADR 0105: A serial port of the computer is a digital device in the engine

Date: 2026-10-27. Status: accepted (task SN-237).

## Context

The owner asked for a part that connects a real serial port of the computer
(a USB-serial adapter, or a virtual port of another program) to RX, TX and
GND of a simulated circuit, with the port, baud rate and format chosen in the
part. Firmware on a simulated Blue Pill should then talk to a terminal
program, or to the student's own program, as a real board would.

## Decision

- The part is a **digital device** (ADR 0102, kind `serial_port`): its logic
  decodes frames from the RX pin's edges at the middle of each bit and sends
  the computer's bytes on TX as frames at the chosen settings. On a board's
  nets the device bus resolves the edges at once, so a USART receiving from
  the part decodes any baud rate without *Exact microcontroller edges*
  (`MachineOptions::exact_input_pins`); without a board it runs in a chip
  group (ADR 0103) and its input sees the circuit's window.
- The **device opens the port itself**, in the engine process, through a
  small Qt-free layer (`src/devices/serial_port`: Win32, termios). The port
  is read without waiting every millisecond of simulated time; written bytes
  go to the system's buffer. No new dependency, no new engine protocol
  message, and the same part works in `simnodus-cli` runs.
- The port opens when a run starts and closes when it ends; opening a
  project opens nothing. A missing or busy port is a message and the run goes
  on: projects keep the port's name and move between computers.
- The level (3.3 or 5 V) is a parameter, not a supply pin
  (`DigitalDevice::supply_parameter`), like the jumper of an adapter.
- `loopback` is a built-in port that sends back what it receives, for tests
  and for trying a circuit without an adapter.

## Alternatives

- **The window owns the port (Qt Serial Port) and relays bytes** through new
  engine protocol messages, like the external I/O link: a new Qt module to
  ship and keep in CI, a relay path with its own latency, and no port in CLI
  runs.
- **Bytes straight into the USART** (as board-to-board serial links do):
  exact and cheap, but only for a board's USART pins; a level shifter, a
  MAX232 or a bit-banged UART between them would not work.

## Consequences

- The coupling is in simulated time: at real-time speed both sides keep
  pace; a slower run passes the computer's bytes on later (up to 64 KB wait,
  then a message), a faster one sends the circuit's bytes sooner than their
  frames would on a wire. Runs with a real port are not repeatable.
- A part with an open port polls every simulated millisecond: an event per
  millisecond in its runner, negligible next to the firmware.
- No RTS/CTS or DTR/DSR: the port's flow control is off and DTR/RTS are
  raised as most adapters expect. Break conditions are not sent.
- Real ports can only be checked on a computer with an adapter or a virtual
  port pair; the automated tests use `loopback`.
