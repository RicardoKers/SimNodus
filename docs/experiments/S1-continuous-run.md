# S1: Continuous interactive ngspice run

Date: 2026-10-14. Task: SN-103. Decision input for
[ADR 0080](../decisions/0080-continuous-interactive-simulation.md).
**Result: segmented runs with capacitor initial conditions are recommended;
a single unbounded transient is not viable.**

## Questions

1. How fast does memory grow during one long transient, and does an output
   option bound it?
2. Can the data callback pace the run to real time × speed factor, and how
   quickly does an external control (push button) reach the circuit?
3. Does restarting the circuit in segments, carrying capacitor voltages as
   initial conditions, keep memory flat, and how large are the discontinuity
   and restart cost?

## Setup

- Windows 11 (build 26300), Intel Core i7-11370H, 32 GB RAM; MSVC 19.51,
  Release build; ngspice-47 DLL from the pinned package
  ([baseline](../../tools/backend-baseline.json)), loaded at run time through
  `src/sim/ngspice_runtime` with the owned `spinit`.
- Circuit ([source](../../tests/spikes/s1_continuous.cpp)): an external
  voltage source `Vbtn` (the button) drives a voltage-controlled switch that
  charges 10 µF through 1 kΩ from 3.3 V (τ = 10 ms); a red LED (diode model)
  with 100 Ω discharges the capacitor; 100 kΩ leakage. `.options reltol=1e-4`.
- Deterministic button schedule for comparisons: pressed for 50 ms every
  200 ms from 20 ms. The pacing case instead toggles the button from a "user"
  thread every 250 ms of wall time.
- Every scenario runs in a fresh process: `python tests/spikes/run_s1.py`.
  Memory is the process commit charge (private bytes).

## Results (Windows, ngspice-47)

### One long transient

| Max step | Saved vectors | Points per sim. s | Unpaced speed | Memory growth |
|---|---|---|---|---|
| 1 µs | all | 1 000 000 | 0.61× | 72 MB per simulated s |
| 1 µs | `.save out btn` | 1 000 000 | 0.63× | 27 MB/s |
| 10 µs | `.save out btn` | 100 000 | 7.3× | 0.64 MB/s |
| 100 µs | `.save out btn` | 10 000 | 55× | 0.31 MB/s |
| 100 µs | `.save` + `interp` | 10 000 | 56× | 0.33 MB/s |

- **ngspice refuses very long runs.** With `.tran 1u 1000 0 1u`, ngspice-47
  estimates the output memory as nodes × tstop/tstep (59.6 GB here), prints
  `Error: memory required ... is more than the memory available` and then
  *"ngspice.dll cannot recover and awaits to be reset or detached"*. A "very
  long stop time" therefore only works with a coarse output step, and the
  estimate is preallocated: a 1000 s stop with a 1 ms output step committed
  about 61 MB before the first point.
- Memory grows linearly with simulated time in every configuration; `.save`
  reduces it by about 2.7×, `interp` does not help. At 100 µs that is still
  about 1.1 GB per simulated hour.
- A 1 µs maximum step cannot run in real time on this machine (0.6×).

### Paced run with a button

| Max step | Speed | Achieved / requested | Max lag | Button → accepted point (wall) | Memory |
|---|---|---|---|---|---|
| 100 µs | 1× | 0.9986 | 21 ms | median 64 µs, max 0.35 ms | 0.84 MB/s |
| 10 µs | 1× | 0.9987 | 19 ms | median 47 µs, max 0.19 ms | 0.84 MB/s |
| 1 µs | 0.1× | 0.9989 | 27 ms | median 58 µs, max 0.20 ms | 8.4 MB/s |
| 100 µs, `timeBeginPeriod(1)` | 1× | 0.9986 | 27 ms | median 70 µs, max 2.2 ms | 0.65 MB/s |

- Pacing inside the `SendData` callback works: the run thread sleeps when it
  is ahead of wall time × speed. The engine was asleep 94 % of the time at
  100 µs and 59 % at 10 µs, which is the headroom for heavier circuits.
- The worst lag (19–27 ms) comes from late wake-ups of `sleep_until` and did
  not improve with a 1 ms timer resolution. It is below one frame of the
  10–30 Hz live display.
- A button change reaches the next accepted point within one step: in
  simulated time the latency was one step (median 100 µs at 100 µs maximum
  step). External source values are read at trial times, so a control edge is
  resolved to the step size, not exactly.

### Segmented runs

Segments restart the same netlist with `C1 IC=<final voltage>` and `uic`; the
button schedule is offset by the segment start. Compared with one continuous
reference run on a 0.5 ms grid:

| Max step | Segment | Max difference after a restart (first 5 ms) | Max difference anywhere | Restart cost |
|---|---|---|---|---|
| 100 µs | 0.5 s | 0.21 mV | 17.3 mV | < 0.5 ms median |
| 100 µs | 1 s | 0.11 mV | 17.3 mV | < 0.5 ms median |
| 100 µs | 2 s | 0.11 mV | 17.3 mV | < 1.1 ms max |
| 10 µs | 1 s | 0.011 mV | 1.7 mV | < 1.9 ms max |

- The restart itself is practically invisible (≤ 0.21 mV at a 3.3 V scale).
  The larger "anywhere" difference is at button edges (for example 1.0205 s,
  0.5 ms after a press, 20 ms after the restart): the two runs place their
  time steps differently, so the edge moves by up to one step. It scales with
  the maximum step (17 mV at 100 µs, 1.7 mV at 10 µs) and is unrelated to the
  carried state.
- **Memory stays flat.** A soak of 300 one-second segments (5 simulated
  minutes, no stored traces) kept private memory between 3.95 and 4.16 MB
  (trend over the second half: +0.001 MB per simulated second). 60 segments at
  10 µs: 4.5–5.3 MB, no trend. Restart (`remcirc`, `destroy all`, load):
  median 0.24 ms, maximum 0.95 ms at 100 µs.

## Results (Linux, ngspice-42, CI quick matrix)

GitHub-hosted `ubuntu-24.04` runner (image 20261004.327), GCC 13.3, Ubuntu's
`libngspice0` (ngspice-42), [CI run 38559847476](https://github.com/RicardoKers/SimNodus/actions/runs/38559847476).
Memory is the data segment from `/proc/self/statm`, so absolute values are not
comparable with Windows commit charge.

| Scenario | Result |
|---|---|
| One long transient, 100 µs, `.save` | 39× unpaced, memory +0.16 MB per simulated s (grows) |
| Paced, 100 µs, 1× | 1.0001 of requested speed, max lag 0.2 ms, button → accepted point median 0.73 ms (max 1.1 ms), one step in simulated time |
| Segments 1 s vs continuous, 100 µs | 0.11 mV after restarts, 17.3 mV at button edges (same as Windows) |
| Soak, 60 segments of 1 s, 10 µs | memory 17.4 → 20.5 MB during the first half, then flat; restart median 0.55 ms |

Linux timers wake up within 0.2 ms, so the ~20 ms lag on Windows is a property
of Windows sleep granularity, not of the pacing method.

## Recommendation (recorded in ADR 0080)

1. Interactive mode runs **segments of 1 s simulated time** (configurable
   0.5–2 s), each a `.tran` with output step = maximum step and stop time =
   segment length, so ngspice's output estimate stays small. Between
   segments: copy the final values, `destroy all`, `remcirc`, load the next
   segment with initial conditions for **every capacitor voltage and inductor
   current**, `uic`.
2. Default maximum step **100 µs** for interactive mode (55× real-time
   headroom on this machine), with 10 µs selectable for faster circuits;
   1 µs is a transient-mode setting, not real time. The status bar shows the
   achieved factor (SIM-01).
3. Pace in the data callback against wall clock × speed; report the achieved
   factor and tolerate ~25 ms wake-up jitter.
4. Controls are `external` sources read through `GetVSRCData`; the value must
   be a function of the requested trial time, not of the call (E-01).
5. Use `.save` for the probed vectors only; the segment plot is discarded
   anyway, acquisition lives in SimNodus ring buffers (SIM-02).

## Limits of this spike

- One linear RC with a switch and an LED. Nonlinear device charge (junction
  capacitance, transit time) is not carried across segments; circuits with
  `CJO`/`TT` will show small steps at restarts. SN-118 must measure one
  semiconductor-heavy example.
- Inductor current carry-over was not exercised (no inductor in the circuit).
- Linux numbers come from the four-scenario quick matrix on a shared CI
  runner; see [S2](S2-ngspice-platforms.md) for engine version differences.
- Numbers are from one developer laptop; classroom PCs may be slower.
