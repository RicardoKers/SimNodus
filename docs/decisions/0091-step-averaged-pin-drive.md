# ADR 0091: Firmware outputs enter the circuit as a step-averaged drive

Date: 2026-10-16. Status: accepted (SN-176).

## Context

A firmware pin drives the circuit through the pin model of ADR 0084: a
conductance and a current that the B sources of the board read from the
session at each ngspice trial time. Until now the session returned the drive
in force at the trial time, so an edge acted at the circuit's next time point.
With the default 100 µs maximum step a 1 kHz, 25 % PWM (example 13) averaged
20–30 % in an RC filter (0.66–0.99 V instead of 0.825 V), depending on where
the edges fell between time points. Timers (SN-177, ADR 0090) make kHz PWM
common, and the owner's STM32CubeIDE projects use it.

The own emulator knows every edge to the nanosecond before the circuit reaches
it (the coupling of SN-173 keeps it at least at the circuit's time).

## Decision

For each trial step from the last accepted point t0 to the trial time t, the
session asks the runner for the pin's drive **averaged over (t0, t]**
(`McuBackend::average_drives`). A step that contains edges gets the charge the
pin delivered during the step; a step without an edge gets the drive at its
end, as before. Renode, which only knows levels late, keeps the drive at t.

## Alternatives

Measured on 2026-10-16 (Windows laptop, release build, maximum speed):

| Approach | Example 13: V(RC), 1 kHz 25 % | Lesson 1 ripple (expected 0.805 V) | Example 13 speed | Lesson 1 speed |
|---|---|---|---|---|
| Drive at the trial time (before) | 0.66–0.99 V | 0.79–0.80 V | 2.0x | 1.84x |
| ngspice breakpoints at each edge (`ngSpice_SetBkpt`) and steps capped at the firmware's time (the GetSyncData callback) | 0.8243 V | 0.805 V | 1.0–1.1x | 0.85x |
| **Step-averaged drive** | **0.8251 V** | 0.784 V | **2.0x** | **2.0x** |
| 10 µs maximum step, drive at the trial time | 0.825 V | — | 0.3x | — |

- Breakpoints give exact edge times and shapes. But ngspice restarts with a
  tenth of the step after each breakpoint (about 11 extra time points per
  edge), and the circuit has to wait for the firmware at every window, so the
  two no longer run in parallel. Lesson 1 fell below real time. Restarting at
  the full step after an edge breakpoint helped (example 13 at 1.4x) but not
  enough.
- A smaller maximum step costs speed everywhere.
- Finite rise times and logic-only nets (MCU_EMULATOR.md section 7) remain
  options for serial frames (SN-175, spike SN-186).

## Consequences

- PWM averages are exact at any step: LED brightness, RC filters and motor
  drives see the programmed duty cycle. Speed is unchanged.
- An edge is spread over the circuit step that contains it (at most the
  maximum step, 100 µs by default): the scope shows one point between the
  levels, and peaks of fast RC responses read slightly low (lesson 1: 0.784 V
  measured from the points against 0.805 V).
- Signals much faster than the step (a 115200-baud frame: 8.7 µs bits) are
  averaged away in the analog view; the logic lanes still show the exact
  edges. Serial frames on the pins need SN-175/SN-186.
- The breakpoint code was measured and removed; this ADR keeps the numbers.
  ADR 0093 brought it back as the setting "Exact microcontroller edges" after
  the owner's test of a 1 kHz PWM.

## Revisit criteria

A lesson that needs edge shapes inside 100 µs in the analog view, or serial
frames on the pins (SN-175): then breakpoints only for the nets that need them,
or rise times, measured as in the table above.
