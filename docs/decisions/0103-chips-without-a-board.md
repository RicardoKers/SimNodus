# ADR 0103: Chips without a board run in a group with an idle core

Date: 2026-10-20. Status: accepted (task SN-212; amends ADR 0102).

## Context

After 0.6 the owner asked for parallel ROM and RAM of several sizes, to use
with a microcontroller and later in digital circuits. A ROM as a lookup table
behind a counter, or a RAM written from switches, has no board; ADR 0102 runs
device logic only in a board's runner, so a chip on no board pin did nothing
(with a message). SPICE cannot hold a memory of kilobytes (ADR 0099 keeps one
bit in two capacitors), and XSPICE's `d_ram` has the segment problem measured
in ADR 0099.

## Decision

- Chips on no board pin form **groups**: the chips joined by nets that are
  not ground or a supply, found like a board's devices. Each group runs in a
  runner of its own (`McuRunOptions::chips_only`, protocol version 7), named
  after its first chip.
- The runner's machine runs a built-in two-instruction program (`wfi`, branch
  back): the core sleeps, so the cost is the devices' events and the coupling
  windows. Everything else is the board's machinery unchanged: the device bus,
  device outputs driving the circuit, logic lanes (`U2.D0`), live parameters,
  memory contents for the Memory Viewer.
- Nets of a group have no board pin, so a net with other parts on it (a
  counter, a switch, an LED) is read from the circuit, like a PCF8574 pin since
  SN-205: inputs arrive up to one window (0.1 ms) late. A net between two chips
  stays digital and is resolved at once.

## Alternatives

- **A device loop without a machine:** less to run, but a second
  implementation of timing, windows and outputs next to the board's.
- **Memories in SPICE:** two capacitors and sources per bit; 64 KB would be a
  million elements.
- **Keep chips board-only:** ROM and RAM would not work in digital circuits,
  which the owner listed as a later use.

## Consequences

- Every chip works without a board; the 74HC595 and the others can be driven
  by switches or counters. Their inputs from the circuit are up to 0.1 ms late,
  so they suit clocks up to a few kHz; with a board, nets between board and
  chip stay instant.
- A group with no circuit at all (a chip wired to nothing) runs and does
  nothing; the message that a chip needs a board is gone.
- Each group costs a runner thread with an idle core; a circuit with many
  separate chip groups pays that per group.
