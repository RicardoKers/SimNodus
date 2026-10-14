# Current state

Updated: 2026-10-14. Keep this file short: present state, last results, next
steps. History of the first development line (2026-08-31 → 2026-10-06) is in
[archive/2026-10-legacy/CURRENT.md](../archive/2026-10-legacy/CURRENT.md).

## Where we are

- **Release 0.1.0 published on 2026-10-14** (due 2026-10-30): tag `v0.1.0`,
  GitHub release with `SimNodus-0.1.0-windows-x64.zip`. Phase 1 (SN-110..SN-123)
  is done; the owner's retest of the build passed.
- Everything is merged in `main`: phase 0 (PR #97), phase 1 (#98–#100),
  fixes from the owner's first test (#101: open-switch leakage, crash when
  leaving a value field) and 0.1 polish (#102: crash recovery, net names on
  hover, differential probes, approximate-mode notes in exports), then
  Help > Keyboard Shortcuts (F1, read from the menus).
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

1. Fix whatever classroom use of 0.1.0 brings up (patch releases 0.1.x).
2. Phase 2 (release 0.2, until 2026-11-13; may start early): SN-130 Renode
   engine wrapper first, then Blue Pill, GPIO/EXTI, UART terminal, logic lanes.
3. Carry semiconductor junction charge across segments only if the owner
   sees steps (not seen in the retest).

## Known constraints

- Release zips should be built locally with Qt 6.11.1
  (`tools/package_windows.py`); CI artifacts use Qt 6.10.2.
- Semiconductor junction charge is not carried across interactive segments.
- Windows sleep granularity gives up to ~25 ms pacing lag (invisible at
  20 Hz display).
- QTest mouse-move events are not delivered offscreen; GUI tests send them
  explicitly.

## Resuming in a new session

- **Waiting for:** nothing; continue with "Next steps". Release builds:
  configure a separate tree with `-DSIMNODUS_VERSION_SUFFIX=` (empty; `main`
  stays `dev`), package it, tag `vX.Y.Z`, attach the zip to a GitHub release.
- **Local folder:** `D:_Projects_Actives\SimNodus_project` is a Git
  clone on `main`, in sync with GitHub. `handoff/` and `START_HERE.md` are
  local only.
- **Toolchain (owner's laptop):** VS 18 Build Tools (MSVC 14.51), Qt 6.11.1
  `C:\Qt.11.1\msvc2022_64`, CMake/Ninja from the Build Tools. Run
  commands in a developer shell (`vcvars64.bat`; it needs
  `C:\Program Files (x86)\Microsoft Visual Studio\Installer` on `PATH`).
- **Build and test:** `python tools/fetch_backends.py` (once), then
  `cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64`, `cmake --build build/dev`,
  `ctest --test-dir build/dev` (8 suites), `python tools/check_repository.py`.
- **Package:** `python tools/package_windows.py --build-dir build/dev` writes
  `dist/SimNodus-0.1.0-dev-windows-x64.zip` and runs the packaged
  `--check-engine` and `--smoke-test`. Screenshots for checks:
  `simnodus.exe examples/01_LED_and_push_button.snx --run --screenshot out.png`.
- **Publishing:** `main` requires a pull request with the checks `Build
  (Linux)` and `Build (Windows)`; only squash merges are allowed. The owner
  authorized direct merges once the checks pass.
- **Pitfalls met so far:** Qt 6.4's moc (Linux CI) cannot parse C++20
  standard headers, so Q_OBJECT headers include project headers only under
  `#ifndef Q_MOC_RUN`; GCC treats missing aggregate initializers and implicit
  `[=]` captures as errors; never delete a widget inside its own signal
  (defer with `QTimer::singleShot(0, ...)`); offscreen GUI tests must send
  mouse-move events explicitly.
