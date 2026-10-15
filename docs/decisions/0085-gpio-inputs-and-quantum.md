# ADR 0085: GPIO inputs, EXTI and the coupling quantum

Date: 2026-10-15. Status: accepted.

## Context

SN-133 (MCU-04) closes the loop: circuit voltages must reach GPIO inputs and
fire EXTI interrupts, so that a push button on the schematic triggers the
firmware. E-03 established that this direction can only be a sampled
approximation: the firmware has already run past the circuit's time when
the circuit's change is known. MCU-02 asks for a coupling quantum Q chosen
from measurements and configurable. Two Renode model limits surfaced while
building it: the STM32F1 GPIO model reset CRL/CRH to "analog" and ignored
BSRR/ODR writes for input pins, so the pull-up/pull-down choice was lost.

## Decision

- **Coupling quantum Q** (project setting `mcu_quantum`, default 1 ms,
  0.1 ms to 20 ms, in Simulation Settings): the firmware runs at most Q ahead
  of the circuit and advances in chunks of Q. Inputs observed by the circuit
  at time t reach the firmware before its next chunk, so their delay is at
  most Q (measured: max 1000 µs at Q = 1 ms; button edge to LED edge
  1.1 ms through EXTI and the output).
- **Schmitt trigger per input** on the pin voltage to the board ground,
  thresholds from the STM32F103 datasheet guarantees: 1 above 1.83 V
  (V_IH min at 3.3 V), 0 below 1.16 V (V_IL max), unchanged between. Only
  pins configured as inputs receive the level; the runner keeps the wanted
  level and applies it when a pin becomes an input.
- **Own GPIO model** `resources/renode/SimNodusGpio.cs` (derived from
  Renode's MIT STM32F1GPIOPort, compiled by Renode at start-up): CRL/CRH reset
  to 0x44444444, ODR is a real register (pull selection readable), and a pin
  that becomes an output drives its ODR level. The board's `extensions`
  list loads it before the platform.
- **EXTI** lines 0–15 receive pin n of ports A–C; lines 0–4 go to NVIC IRQ
  6–10, 5–9 and 10–15 to the combined IRQs 23 and 40, as on the chip. AFIO
  port selection (`AFIO_EXTICR`) is not modeled: line n follows pin n of
  every port.
- **Renode speed:** the board profile enables
  `emulation SetGlobalAdvanceImmediately true`, which skips the CPU's idle
  time (WFI) without changing virtual time. Measured with the blink firmware:
  0.71x → 1.43x real time with 1 ms chunks, 0.96x → 4.21x with 10 ms; the
  event times were unchanged. Bus reads cost about 0.1 ms each, so pin
  configurations are read every 10 ms of virtual time.

## Update (SN-136)

- Renode can raise GPIO events of a run chunk after the RunFor returned (its
  CPU thread finishes the quantum): a whole chunk of PA1 edges reached the
  circuit 2 ms late in about one run of seven, and the upstream server
  dropped events outside a RunFor. The server now holds such events for the
  next RunFor, and the runner reports a chunk as complete only after the
  next one; it may run one chunk past the circuit's limit to keep going. The
  firmware therefore leads by up to two quanta and inputs arrive up to two
  quanta late (measured: 2000 µs at Q = 1 ms). 0 failures in 15 repeated
  lesson 1 runs afterwards.
- Input changes are queued and replayed with their spacing (pulses shorter
  than Q survive).
- Speed of example 08 without the extra round trip: 1.00x real time at
  Q = 1 ms (a zero-length "completing" run per chunk had cost 25 %).

## Alternatives

- *Exact causal input timing* (rewinding the firmware or a Renode extension
  that blocks at input edges): rejected for now, as in ADR 0011; a smaller Q
  reduces the delay when a lesson needs it.
- *Q = 0.1 ms by default*: run at about 0.4x real time on the development PC
  even for idle firmware; 1 ms keeps real time with margin.
- *Patching values from outside instead of an own model*: writes do not
  survive Renode's deferred reset (ADR 0083) and cannot recover ODR.

## Consequences

- Input timing is approximate within Q; the run's approximation note states
  Q, the thresholds and the 100 µs output quantum (SIM-09).
- Firmware that is busy (no WFI) runs slower than the idle examples; the
  circuit waits for it, so the run's real-time factor drops, never the
  timing consistency.
- The own GPIO model must follow Renode upgrades; its differences are listed
  in its header. ADR 0083's GPIO reset deviation no longer applies.

## Revisit criteria

Lessons needing sub-millisecond input timing (capture, UART on wires),
AFIO-dependent pin assignments, or a Renode version with fixed GPIO models.
