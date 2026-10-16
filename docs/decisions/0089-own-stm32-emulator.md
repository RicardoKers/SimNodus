# ADR 0089: Own STM32 emulator as the microcontroller engine

Date: 2026-10-15. Status: **accepted** on 2026-10-15 (decision D0, owner),
after experiment E-07 met the criteria of gates G0a and G0b
([E-07-results.md](../experiments/E-07-results.md)).

## Context

Firmware runs in Renode 1.16.1 (ADR 0082–0088). That path works and ships in
release 0.1/0.2, but its limits shape every MCU feature:

- 7–9 s start per run; a .NET child process; a 137 MB package.
- Coupling through a loopback protocol in quanta (1 ms default), timestamps on
  Renode's 100 µs step, events only during `RunFor` (ADR 0083, 0085, PR #116).
- Byte-level UART without pin waveforms (ADR 0086; owner's question on
  2026-10-15: why the scope shows nothing on PA9), owned extensions needed
  for ADC (E-04), an audit pending for timers (SN-141), a clock model written
  in C# (ADR 0088), one instruction per cycle.
- Debugging needs a patched, cancellable Renode build (E-05, SN-150).

The owner proposed writing SimNodus's own emulator, starting with the
STM32F103 and later other chips (STM32F407), with the Blue Pill as a board
around the chip.

## Decision (proposed)

Build an in-process STM32 emulator in `src/emu` (C++20, MIT, no third-party
emulator code), designed in [MCU_EMULATOR.md](../architecture/MCU_EMULATOR.md)
and delivered by the [plan](../planning/MCU_EMULATOR_PLAN.md):

- interpreter for ARMv7-M Thumb-2 with predecoded instructions; exceptions,
  NVIC, SysTick and SCB as on the Cortex-M3;
- pin-level STM32 peripherals with cycle timestamps converted to nanoseconds;
- the same runner interface as today (`McuBackend`), so the session and the
  GUI do not change; engine selection per board profile;
- Renode stays the shipped engine until gate G1 and a fallback at least until
  decision D4; features built only on the emulator (after D2) are missing from
  that fallback, which decision D3 states explicitly.

The decision becomes **accepted** only if the prototype passes gates G0a and
G0b (instruction tests, random differential blocks with full decoder coverage
against Renode and a third reference, every example firmware, FreeRTOS, faults
and resets, at least 150 MIPS on Windows and Linux, ELF load under 50 ms, the
owner's CubeIDE project). Otherwise it is rejected or deferred with the measured reasons.

## Alternatives

- *Stay with Renode and extend it* (C# peripheral models, a patched build for
  debugging): keeps the start time, the quantum coupling and the package size;
  pin-level UART and exact edges would need changes deep in Renode's timing.
- *QEMU or Unicorn as the CPU*: GPL-2.0, incompatible with linking into the
  MIT application (AGENTS.md: no GPL-only libraries linked in); as a separate
  process it keeps the coupling problems of Renode.
- *Another permissive emulator core*: none found that covers ARMv7-M
  exceptions and is maintained with a compatible license; to be rechecked in
  E-07 before writing the core (a found candidate would change the plan, not
  the goals).
- *A just-in-time compiler*: faster but much harder to validate; only if the
  interpreter fails the performance gate.

## Consequences

- Large, self-owned code: correctness rests on the differential tests,
  instruction tests, the firmware corpus and the owner's real board.
- New MCU features after G1 are written once, on the emulator (plan principle 3).
- The calendar of releases 0.3 and 0.4 depends on decisions D1–D3; the
  classroom release falls back to Renode if the emulator is not ready (D3).
- Supporting more chips becomes our work: core variants (Cortex-M4F) and
  peripheral versions, reused across the STM32 family.

## Revisit criteria

Gate G0 result; any gate failing twice; a permissive emulator core appearing
that covers the same scope; classroom PCs too slow for the interpreter.
