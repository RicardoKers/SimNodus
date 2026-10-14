# S2: ngspice on Windows and Linux

Date: 2026-10-14. Task: SN-104.
**Result: pending the first CI comparison** (the Windows part passed; see
below).

## Question

Does the application load the bundled ngspice DLL at run time on Windows, and
does the same netlist give the same result with ngspice-47 on Windows and the
distribution ngspice on Linux (Ubuntu 24.04 ships ngspice-42)?

## Criteria (declared before the first cross-platform run)

Implemented in [s2_compare.py](../../tests/spikes/s2_compare.py):

- Analytical gate for `rc_step` and `rlc_ring`: every sample of the 501-point
  uniform grid within 0.5 % of full scale of the analytical response (E-01).
- Cross-run gate for every probed vector of the four
  [reference netlists](../../tests/spikes/s2): RMS difference below 0.1 % and
  maximum difference below 1 % of the vector's range in the reference run.
  The maximum is looser because switching edges fall between grid samples
  differently when engines choose different time steps.

## Netlists

| Netlist | Content | Probes |
|---|---|---|
| `rc_step` | 3.3 V step, 1 kΩ, 1 µF, `uic` | `out` |
| `rlc_ring` | 5 V step, 50 Ω, 10 mH, 100 nF (underdamped) | `out`, `l1#branch` |
| `diode_clamp` | 5 V 1 kHz sine, 1 kΩ, silicon diode with junction capacitance | `in`, `out`, `v1#branch` |
| `bjt_led` | 1 kHz pulse into an NPN switch driving a red LED from 5 V | `base`, `col`, `vled#branch` |

## Windows (ngspice-47, run-time loaded DLL)

- `src/sim/ngspice_runtime` loads `ngspice.dll` with `LoadLibraryExW` and
  `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32`, so
  `sndfile.dll`, `samplerate.dll` and `libomp140.x86_64.dll` are found only
  next to the engine. That search mode requires an absolute path (a relative
  one fails with Windows error 87). `SPICE_SCRIPTS` points at the owned
  `spinit` before loading.
- The packaged application (`simnodus.exe --check-engine`) loads
  `ngspice/ngspice.dll` from its own folder and solves a divider exactly
  (1.650000 V), also when started from a fresh folder with `PATH` reduced to
  `System32`.
- Reference results: [reference-windows-ngspice47.json](../../tests/spikes/s2/reference-windows-ngspice47.json)
  (accepted points: `rc_step` 5012, `rlc_ring` 5081, `diode_clamp` 3094,
  `bjt_led` 4101; total solve time 28 ms). Analytical errors: `rc_step`
  33 µV, `rlc_ring` 1.9 mV (gates 16.5 mV and 50 mV). A repeat run was
  bit-identical.
- The `rc_step` accepted points match E-01 exactly (5012 points, 0.0989 µV
  maximum error against the analytical response at accepted points).

## Linux (distribution ngspice)

Every CI run compares the Linux result with the committed Windows reference
(ctest `spike-s2-reference`) and, in the `S2 cross-platform comparison` job,
with the Windows result of the same run. Results are recorded here after the
first CI run of this branch.

## Consequences

- Windows packages bundle ngspice-47 with its three DLL dependencies in
  `ngspice/`; nothing is searched on `PATH`.
- Linux development uses the distribution library; its version is shown by
  `--check-engine` and in the Messages panel.
