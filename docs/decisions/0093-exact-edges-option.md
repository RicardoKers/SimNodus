# ADR 0093: Exact microcontroller edges as a simulation setting

Date: 2026-10-16. Status: accepted (owner, 2026-10-16).

## Context

ADR 0091 made each circuit step take the pin's drive averaged over the step:
PWM averages are exact and runs stay fast, but an edge is spread over the
step that contains it. The owner tried a 1 kHz PWM: at the default 100 µs step
the waveforms on the scope are deformed; at a 10 µs step they are right but
the run falls to a quarter of real time ("not a big problem, considering the
complexity of the simulation"). ADR 0091 had measured a third way, ngspice
breakpoints at every firmware edge, and rejected it as the default only
because lesson 1 ran below real time.

## Decision

- A project setting **Exact microcontroller edges (slower)**
  (`SimulationSettings::exact_edges`, `"exact_edges"` in the project file,
  off by default) in Simulate > Simulation Settings.
- On, for each board on the own emulator: at every accepted point the session
  waits until the firmware has reached the window boundary below its limit,
  asks for the drive changes of the wired pins up to there
  (`McuBackend::drive_changes`) and sets an ngspice breakpoint 0.7 ns before
  each (`ngSpice_SetBkpt`), so a time point reads the old level and the next
  the new one. The steps are capped at that boundary through the GetSyncData
  callback (`NgspiceSession::set_step_handler`), since later edges are not
  known yet; after an edge breakpoint the step goes back to the maximum step
  instead of ngspice's tenth (its error control still shortens it).
- Off: unchanged (ADR 0091). Renode boards are never fed (they report edges
  late).
- The GetSyncData callback is registered for every run (ngspice cannot
  unregister it); without edge feeds it changes nothing.

## Consequences

Measured 2026-10-16 (Windows laptop, maximum speed, 100 µs step):

| Example | Off | On |
|---|---|---|
| 07 blink | 2.8x | 2.9x |
| 10 lesson 1 (1 kHz square into RC) | 2.9x, ripple 0.58 V read from the points | 1.2x, ripple 0.806 V (formula 0.805 V), no points between the levels |
| 12 HAL at 72 MHz | 1.2x | 1.7x |
| 13 1 kHz PWM | 2.7x | 1.5x |
| 14 ADC to PWM | 1.6x | 0.8x |
| 15 HAL ADC + DMA | 1.0x | 1.2x |

A 10 µs step gave the owner the same waveforms at 0.25x. With the setting on,
the circuit waits for the firmware at every window, so the two no longer run
in parallel; this costs most where the firmware is busy (example 14).

## Revisit criteria

Turn it on by default if classroom PCs keep the lessons above real time with
it, or make it per board if mixed projects need it.
