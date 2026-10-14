# Current state

Updated: 2026-10-14. Keep this file short: present state, last results, next
steps. History of the first development line (2026-08-31 → 2026-10-06) is in
[archive/2026-10-legacy/CURRENT.md](../archive/2026-10-legacy/CURRENT.md).

## Where we are

- **Phase 0 — Reset is complete** (SN-100..SN-104). Next: phase 1, release 0.1
  on 2026-10-30.
- Work is on branch `rebuild/phase-0` of `RicardoKers/SimNodus` (not merged to
  `main` yet). The local folder is a Git working copy of that branch.
- Previous product code is in [legacy/](../../legacy/README.md) (not built by
  default; still builds and passes 67/67 tests with `cmake -S legacy`).
- The new application `apps/simnodus` is a skeleton: three adjustable,
  collapsible columns (component tree with search and preview, schematic
  canvas with dot grid/zoom/pan, properties), messages panel, status bar,
  menus with planned actions disabled, separate Signal Analyzer window with an
  empty scope, persisted geometry. No placing, wiring or simulation yet.
- `src/sim` loads ngspice at run time (Windows DLL bundled; Linux system
  library). The only engine entry point in the GUI is Tools > Check
  Simulation Engine.

## Verified on 2026-10-14

- Windows 11 (owner's laptop: MSVC 19.51, Qt 6.11.1 msvc2022_64, CMake
  4.2.3, Ninja, ngspice-47 and Renode 1.16.1 fetched and SHA-256 verified):
  build without warnings (`/W4 /WX`), ctest 6/6 (unit, engine, S1, S2,
  offscreen GUI 10 cases, executable smoke), repository checker passed.
- Portable zip `dist/SimNodus-0.1.0-dev-windows-x64.zip` (26 MB): extracted
  outside the repository with `PATH` reduced to `System32`, `--check-engine`
  loaded the bundled ngspice-47 (divider 1.650000 V) and `--smoke-test`
  passed on the real desktop.
- CI ([ci.yml](../../.github/workflows/ci.yml), green in
  [run 38560132382](https://github.com/RicardoKers/SimNodus/actions/runs/38560132382)):
  Ubuntu 24.04 (GCC 13.3, Qt 6.4, ngspice-42) and Windows (Qt 6.10.2,
  ngspice-47) build, test and run the S1 quick matrix; the Windows job
  packages the zip, runs its `--check-engine` and `--smoke-test` on the
  runner and uploads it as artifact `simnodus-windows-x64`; S2 compares the
  two platforms.
- [Spike S1](../experiments/S1-continuous-run.md): one long transient grows
  without bound (27 MB per simulated s at 1 µs, 0.3 MB/s at 100 µs) and
  ngspice-47 refuses tstop/tstep estimates beyond free memory; 1 s segments
  restarted with capacitor initial conditions keep memory flat (300 segments,
  3.95–4.16 MB), restart 0.24 ms, ≤ 0.21 mV step; pacing holds 0.999× of
  real time; a button reaches the circuit within one step. Recorded in
  [ADR 0080](../decisions/0080-continuous-interactive-simulation.md).
- [Spike S2](../experiments/S2-ngspice-platforms.md): ngspice-42 (Linux)
  matches ngspice-47 (Windows) within 1.5 × 10⁻⁵ of range on four netlists.

## Next steps (phase 1)

1. SN-110 document model with stable IDs and edit commands.
2. SN-111 library format and component set A; SN-112/113 canvas editing and
   wiring.
3. SN-116/117/118: netlist generation, engine wrapper, interactive run with
   segments (ADR 0080 rules), then SN-119/120 live components and scope.
4. Merge `rebuild/phase-0` into `main` after the owner's review.

## Known constraints

- CI uses Qt 6.10.2 on Windows (aqtinstall cannot install 6.11 yet); release
  zips should be built locally with Qt 6.11.1 (`tools/package_windows.py`).
- Windows `sleep_until` wakes up to ~25 ms late; acceptable for a 10–30 Hz
  display, revisit if pacing jitter becomes visible.
- Semiconductor charge is not carried across segments (S1 limit); SN-118 must
  measure a semiconductor-heavy example.
