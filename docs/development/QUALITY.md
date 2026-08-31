# Quality and validation

## Foundation checks

Run `python tools/check_repository.py` for documentation/link/JSON hygiene. CMake configuration verifies the build scaffold can select a C++ compiler; it does not compile a simulator. The CI workflow currently checks only this foundation.

Local foundation verification on 2026-08-31:

- Repository checker passed for all 54 project text files, including local links and JSON.
- CMake configuration passed with Visual Studio 18 2026, MSVC 19.51.36246.0, and Python 3.14.4.
- The CMake `check-repository` target passed in Debug configuration.
- Repository text was reviewed for the English-only requirement; no private chat exports or runtime binaries were added.
- The initial 54-file staged snapshot passed whitespace checks and a targeted credential/private-path scan before publication. Build output and local publication helpers are ignored.

Hosted verification of the first public commit `b3163a1` passed on both `windows-latest` and `ubuntu-latest`: [run 33399498653](https://github.com/RicardoKers/SimNodus/actions/runs/33399498653). Both jobs checked repository files, configured CMake with a C++ compiler, and ran the check target.

No backend experiment or simulator test has run. Local and hosted results validate the foundation, not application compilation, simulation behavior, or a supported Linux application release.

## Engine verification layers

| Layer | Required evidence |
|---|---|
| Domain | Stable IDs, connectivity, hierarchy, invalid inputs, format round-trip |
| Kernel | Monotonic committed time, simultaneous events, late-event rejection, bounded delta loops |
| Couplers | Finite drivers, high impedance, undefined input, thresholds and units |
| Adapter contracts | Actual advance/stop times, callback ownership, reset/shutdown/failure |
| Real integration | Rebuildable ELF and analog/digital feedback against declared references |
| Debugging | GDB/CubeIDE breakpoint, step, continue, reset, disconnect and overshoot cases |
| Desktop | Save/open/undo, responsiveness, bounded plots, backend crash recovery |
| Packaging | Clean Windows machine, relative paths, offline resources, license notices |

Avoid high coverage targets without meaningful assertions. Test invariants and observed requirements, not private implementation details.

## Numeric and temporal references

Use analytically tractable R/RC/RL circuits first, then known SPICE references and measured board behavior where appropriate. Record initial conditions, solver settings, model provenance, temperature/supply assumptions, sample times, units, and tolerances before evaluating results.

Separate numerical solver error, temporal coupling error, peripheral-model error, and visual downsampling. Stable-looking waveforms do not establish causal correctness.

## Reliability and security

Exercise malformed files, missing models, recursive hierarchy, path escapes, excessive resource use, backend timeout/crash, invalid ELF, and failed saves. A dirty document must survive backend failure. Native models and debugger ports need a reviewed trust boundary.

## Release gates

A source publication needs consistent documentation, MIT attribution, no private material, and a working structural workflow. A binary release additionally needs a tested dependency inventory, supported OS matrix, reproducible examples, known limitations, installation checks, and appropriate third-party notices.

A classroom candidate requires E-06 and an owner go/no-go decision. CI passing does not replace the classroom rehearsal.
