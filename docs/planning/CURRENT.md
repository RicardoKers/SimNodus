# Current state

Updated: 2026-10-14. Keep this file short: present state, last results, next
steps. History of the first development line (2026-08-31 → 2026-10-06) is in
[archive/2026-10-legacy/CURRENT.md](../archive/2026-10-legacy/CURRENT.md).

## Where we are

- **Phase 1 — analog interactive editor (release 0.1, due 2026-10-30):
  feature-complete for review.** SN-110..SN-122 are done; SN-123 (package
  0.1) is waiting for the owner's hands-on test.
- Phase 0 is merged in `main` (PR #97). Phase 1 arrives through PRs #98
  (model, library, files), #99 (netlist) and #100+ (session, GUI).
- What works: place/wire/edit circuits with component set A (20 types),
  undo/redo, `.snx` files with backup, examples; continuous interactive
  simulation (segments per ADR 0080) with switches, buttons,
  potentiometers, glowing LEDs, meter readouts; transient mode; Signal
  Analyzer with probes, roll/trigger, cursors, measurements, CSV/PNG export.
  See the [quick start](../user/QUICK_START.md).

## Verified on 2026-10-14

- Windows 11, MSVC 19.51, Qt 6.11.1, ngspice-47: build without warnings,
  ctest 8/8 — unit (model, library, files, editing, netlist, 34 + 6 cases),
  engine (library components against the real engine, current probes,
  session: button charges a capacitor, continuity and pulse phase across
  segments, 1x pacing, pause, stop, transient accuracy), GUI (12 widget
  cases; 11 editor end-to-end cases including "hold the button → the LED
  lights" in a live run and a transient record in the analyzer), executable
  smoke test. Repository checker passed.
- Screenshots of examples 1, 2 and 4 running: symbols, junctions, probes,
  live scope (2 Hz pulse, RC charge/discharge, measured f = 2 Hz, duty 50 %).
- Portable zip (`dist/`, 26.5 MB) runs `--check-engine` and `--smoke-test`
  from the package. Linux: verified by CI for each PR (GCC 13, Qt 6.4,
  ngspice-42); the Windows CI job uses Qt 6.10.2.
- Earlier evidence: [S1](../experiments/S1-continuous-run.md),
  [S2](../experiments/S2-ngspice-platforms.md), ADR 0080.

## Next steps

1. Owner test of the 0.1 package with own lesson circuits; collect feedback.
2. Polish from feedback (likely: symbol details, keyboard flows, scope
   defaults); differential probe UI (AN-02), approximate-mode label in
   exports (SIM-09).
3. Phase 2 (from 2026-11-02): Renode integration, Blue Pill, GPIO/EXTI,
   UART terminal, logic lanes.

## Known constraints

- Release zips should be built locally with Qt 6.11.1
  (`tools/package_windows.py`); CI artifacts use Qt 6.10.2.
- Semiconductor junction charge is not carried across interactive segments.
- Windows sleep granularity gives up to ~25 ms pacing lag (invisible at
  20 Hz display).
- QTest mouse-move events are not delivered offscreen; GUI tests send them
  explicitly.
