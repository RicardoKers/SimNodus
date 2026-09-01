# Quality and validation

## Foundation checks

Run `python tools/check_repository.py` for documentation/link/JSON hygiene. Root CMake configuration verifies the build scaffold can select a C++ compiler; it does not compile a simulator. The foundation CI workflow checks this layer; separate Windows workflows run E-01, SN-019, and E-02.

Local foundation verification on 2026-08-31:

- Repository checker passed for all 54 project text files, including local links and JSON.
- CMake configuration passed with Visual Studio 18 2026, MSVC 19.51.36246.0, and Python 3.14.4.
- The CMake `check-repository` target passed in Debug configuration.
- Repository text was reviewed for the English-only requirement; no private chat exports or runtime binaries were added.
- The initial 54-file staged snapshot passed whitespace checks and a targeted credential/private-path scan before publication. Build output and local publication helpers are ignored.

Hosted verification of the first public commit `b3163a1` passed on both `windows-latest` and `ubuntu-latest`: [run 33399498653](https://github.com/RicardoKers/SimNodus/actions/runs/33399498653). Both jobs checked repository files, configured CMake with a C++ compiler, and ran the check target.

Those initial hosted results validate the foundation, not application compilation, simulation behavior, or a supported Linux application release. Subsequent E-01 results are recorded separately below.

## SN-010 local Windows checks

The [SN-010 report](../experiments/SN-010-results.md) records 14 verified archive/file hashes, a real C++20 ngspice DLL load/init/version/quit, rejected missing/invalid dependency setups, ignored local user initialization, and Renode headless startup. The ngspice probe compiled with MSVC warnings treated as errors. ARM GCC and GDB version queries passed; firmware compilation/debugging did not run.

The repository checker passed for 62 text files after this change; CMake foundation configuration/check and Python syntax checks also passed locally. C++/header text and the owned `spinit` are now included in text hygiene checks.

Failures are part of the record: the official Renode C client does not build unchanged with MSVC, and ngspice's documented pre-init call crashed the selected DLL. The startup workaround passed, without proving full lifecycle semantics. The optional probe is not built/run in foundation CI and downloads no dependencies by itself.

## SN-011 / E-01 checks

The [E-01 report](../experiments/E-01-results.md) records eight real ngspice cases in three local runs. An independent Python analyzer checks analytical voltage errors, finite/monotonic vectors, exact callback/vector agreement, reset/repeat tolerances, pause behavior, rejected trial points, and invalid-netlist recovery. Deliberately corrupted evidence was rejected. The refactored SN-010 startup probe also compiled and passed.

The local repository checker passed for 75 text files after this change, including `.hpp` and `.cir` fixtures. Python syntax, SVG XML parsing, and whitespace checks passed.

The [native Windows workflow](../../.github/workflows/ngspice.yml) verifies the three pinned ngspice archives before extraction, builds the C++20 host, and runs the suite twice. It prints compact results in the build log. It is separate from foundation CI and does not establish Linux runtime support. Raw local logs/CSV and downloaded packages stay in ignored build output; sanitized metrics and an analytical comparison chart are committed.

The [first hosted E-01 run](https://github.com/RicardoKers/SimNodus/actions/runs/33406697838) passed both suites on Windows Server 2025 / MSVC 19.51.36256.0. [Foundation checks](https://github.com/RicardoKers/SimNodus/actions/runs/33406697777) also passed on Windows and Ubuntu for that revision.

These checks establish only the reported ideal RC/voltage-source profile. Nonlinear/digital models, long-duration leak behavior, and production adapters remain untested. Firmware coupling and forced Renode termination now have their own bounded E-03 evidence; they do not broaden E-01.

## SN-019 Windows control checks

The [SN-019 report](../experiments/SN-019-results.md) records two complete 20-case local runs. Each includes real Renode control using normal and one-byte transfers, six connections, 36 exact time advancements, machine lookup/error recovery, and observed loopback endpoint/PID and shutdown checks. Nineteen separate input/fault cases exercise missing peers, closed/slow/incomplete streams, malformed/fatal frames, error ownership, rejected connection cleanup, and invalid ports. The real time results repeated identically; timeout and handle metrics are reported with their actual limits.

The [dedicated workflow](../../.github/workflows/renode-client.yml) repeats the full suite on Windows. Source and package hashes are verified before generated client/extension use; the original all-interface server is not opened. The checker now includes C source text. These results do not validate firmware, peripheral APIs, in-flight cancellation, coupled timing, Linux execution, or leak endurance.

Local repository verification passed for 88 text files, Python syntax and whitespace checks, and the root CMake configure/check target. The existing eight-case ngspice suite also passed after adding the Renode-only dependency-check option.

[Hosted SN-019 verification](https://github.com/RicardoKers/SimNodus/actions/runs/33437509897) passed both complete suites on Windows Server 2025 / MSVC 19.51.36256.0 / Python 3.12.10. The report links the accompanying passing foundation and ngspice regression checks.

## Coupled GPIO/RC evidence

[E-03](../experiments/E-03-results.md) has a dedicated real-backend host and
[Windows workflow](../../.github/workflows/coupling.yml). The analyzer compares
accepted ngspice samples with the analytical RC, localizes Schmitt crossings,
checks producer/observation/application times, repeats discrete signatures, and
exercises boundary, pulse, late-input, and worker-recreation cases. It reports
sampled exchange as approximate even when every numerical gate passes.

The [hosted E-03 run](https://github.com/RicardoKers/SimNodus/actions/runs/33462555508)
verified the pinned archives, rebuilt the owned firmware and native host, and
passed all 54 isolated cases on a clean Windows runner in 8 minutes 51 seconds.
The foundation checks also passed on Windows and Ubuntu for that revision.

The result does not validate a production scheduler, general causal feedback,
nonlinear circuits, ADC, GDB, or Linux runtime. Keep raw traces outside Git and
commit only the compact summary and a sanitized figure.

## SN-012 / E-02 firmware and I/O checks

The [E-02 report](../experiments/E-02-results.md) records two fresh runs, each with a 100 us and 1000 us profile. The owned ELF built twice identically. The checks cover vector/flash alias, initialized data/BSS startup, exact RunFor endpoints, periodic output, persistent input, both-edge EXTI, a 20 us pulse, same-time input collapse, GPIO modes, missing ADC, loopback ownership, and normal cleanup.

The [dedicated workflow](../../.github/workflows/renode-stm32.yml) verifies the pinned Renode and compiler archives, prepares source-verified control/audit files, rebuilds the firmware and native probe, and repeats both profiles twice. It does not distribute generated artifacts. [Hosted verification](https://github.com/RicardoKers/SimNodus/actions/runs/33441607168) passed all four profile runs with the same ELF hash as the local build; foundation, SN-019, and E-01 regressions also passed for that revision.

Local repository verification passed for 104 text files, Python syntax, JSON parsing, and whitespace checks after E-02. The root CMake check and full SN-019/ngspice regression are repeated before publication.

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
