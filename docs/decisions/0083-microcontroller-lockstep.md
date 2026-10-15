# ADR 0083: Microcontroller in lockstep with the circuit

Date: 2026-10-15. Status: accepted.

## Context

SN-131 runs the Blue Pill's firmware next to the interactive analog run
(ADR 0080). The analog run is paced against wall clock × speed, or runs as
fast as it can at "maximum" speed; Renode advances only in `run_for` and
needs 3.5–8 s to start (ADR 0082). If the two engines run independently,
the firmware drifts behind or ahead of the circuit without bound, which
would make the electrical coupling of SN-132 (E-03 sampled exchange)
meaningless and the pin display misleading.

## Decision

- **One time window, both directions.** The firmware never runs more than
  `mcu_lead_us` (20 ms by default) ahead of the circuit, and the circuit
  waits inside its data callback when the firmware falls more than that
  behind. Each microcontroller has its own `McuRunner` thread that owns its
  `RenodeEngine` and advances in chunks of at most 10 ms up to the limit set
  by the analog time.
- **Start order.** A run with a microcontroller starts Renode first
  (status "Starting the microcontroller...") and starts the circuit only
  when every firmware is ready; a start failure fails the run with Renode's
  explanation. Stop cancels a pending start.
- **Interactive mode only** for now: transient mode refuses boards.
- **Board profiles are library data** (`mcu` section of the component
  JSON): platform description, monitor commands around the ELF load, the
  on-board LED pin and the reset button area. The GPIO pins are recognized
  by name (`PA0`). The reset button writes `SYSRESETREQ` to the SCB AIRCR;
  Renode resets the machine and the loaded ELF survives the reset.
- **Pin display** reads levels from GPIO events and modes from CRL/CRH
  (every 50 ms of wall time).

## Measured model deviations (Renode 1.16.1)

- SysTick counts `RVR` cycles per period instead of `RVR + 1`: the blink
  example's 1 ms tick is 999.875 µs (125 ppm fast).
- STM32F1 GPIO: CRL/CRH read back the last written MODE/CNF fields, but
  reset sets CNF to 00 although the register's reset value is 0x44444444.
  After reset a pin therefore reads as analog instead of floating input;
  the display treats MODE 00 as input (analog becomes relevant with the ADC,
  phase 3). Writing the reset value from outside does not survive Renode's
  deferred reset.
- GPIO change events carry Renode's master time (quantum granularity) and
  are reported only during `run_for` (ADR 0082).

## Alternatives

- *Free-running firmware paced against wall clock*: drifts from the circuit
  at "maximum" speed or when either engine is slower than real time.
- *Strict alternation per quantum Q without overlap*: equivalent to a
  window of Q, but every exchange serializes both engines; the window keeps
  them concurrent and will be narrowed to the coupling quantum in SN-132.
- *C++ board classes*: rejected; the Symbol Editor and new boards should
  only need data.

## Consequences

- At maximum speed the run is as fast as the slower engine. The window
  size bounds the timing error of the later coupling; SN-132 sets it from
  the coupling quantum and labels the result approximate (MCU-02, SIM-09).
- A run with a board takes several seconds longer to start.
- Tests: `renode_tests` (blink firmware timing, runner limit, reset),
  `session_mcu_tests` (window kept in both directions), GUI end-to-end
  (example, pins, LED, reset click).

## Revisit criteria

SN-132 measurements of the coupling error versus window size, or users
needing transient runs with firmware.
