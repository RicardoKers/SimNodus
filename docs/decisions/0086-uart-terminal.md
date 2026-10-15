# ADR 0086: UART terminal through the control protocol

Date: 2026-10-15. Status: accepted.

## Context

SN-134 (MCU-05, AN-08) needs the text a firmware sends on a USART in the
analyzer's terminal, and typed text delivered to its receiver. Renode's
external control protocol has GPIO, ADC and bus commands but no UART.
Renode offers socket terminals and file back ends for UARTs, but a server
socket terminal is another listening port (exposure, ADR 0082) and a file
back end has no input and no timestamps.

## Decision

- **New protocol command `UART` (id 7)** in the loopback server
  (`resources/renode/LoopbackControl.cs`) and the vendored client
  (`renode_get_uart`, `renode_uart_write`, `renode_register_uart_callback`):
  `Write` feeds bytes to the peripheral's receiver (`IUART.WriteChar`);
  `RegisterEvent` reports every transmitted byte (`CharReceived`) with
  Renode's virtual time, like GPIO events, during `RunFor`. Both sides are
  SimNodus code under MIT; the handshake lists the command, so a client and
  server of different versions refuse each other.
- **Platform**: USART1–3 at their STM32F103 addresses with IRQs 37–39
  (Renode `STM32_UART`). Bytes move instantly: the baud rate is not
  simulated, which is labeled in the user guide.
- **Runner**: per UART the newest 1 MiB of transmitted bytes with their
  virtual times; bytes typed in the terminal reach the receiver before the
  next run chunk (at most one coupling quantum late).
- **Terminal** (analyzer tab UART): port choice per board UART, received text
  (CR, LF and CR LF end lines; other control characters show as `<hh>`),
  optional time stamps per line, send line with a selectable line ending.

## Alternatives

- *Renode server socket terminal*: an extra listener; rejected (ADR 0082).
- *Monitor `WriteChar` over stdin and a file back end for output*: no
  ordering with `RunFor`, no timestamps, file polling.

## Consequences

- UART timing is the moment Renode's peripheral handles the byte, not the
  bit timing on a wire; signals on the TX pin are not driven (alternate
  function pins show the configuration only). A wire-level UART would need
  a USART model that drives its pin, a later task if a lesson needs it.
- Measured: the UART example's greeting and "tick 1" at 999.875 ms (SysTick,
  ADR 0083); '1' / '0' typed in the terminal switch the LED on PA0.

## Revisit criteria

Lessons that decode the TX waveform on the scope, need baud-rate errors, or
connect two boards' UARTs by wires.
