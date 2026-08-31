# E-01: Standalone ngspice experiment

Task: SN-011. Predeclared on 2026-08-31, before the first circuit execution. Results are recorded separately in [the E-01 report](../../../docs/experiments/E-01-results.md).

## Inputs and acceptance criteria

- Use the unchanged, hash-verified SN-010 Windows x64 packages and owned initialization file.
- Ideal R = 1000 ohm, C = 1e-6 F, zero capacitor initial voltage, 3.3 V supply. No semiconductor or external native model.
- Baseline: ideal step at time zero, `.tran 1u 5m 0 1u uic`. Compare with `3.3 * (1 - exp(-t / 0.001))` at each accepted sample. Maximum absolute voltage error: 0.0165 V (0.5% full scale).
- External-source case: rising step at 0.001 s and falling step at 0.003 s, ending at 0.006 s; maximum solver step 1 us. Compare against the superposition of two analytical RC step responses. Exclude only samples within 2 us of either discontinuity from the primary error gate; report the error including those samples as well.
- Sample times must be finite and strictly increasing in final vectors; voltages finite. End time must agree with the netlist to within 1 ps. Compare copied callback samples with stored vectors and document any repeated observations across resume.
- Set an integration breakpoint at 2 ms and measure whether it stops execution or only creates a solver boundary. Do not infer a pause from the API name.
- Foreground pause: `stop when time > 0.002`, then `run`, remove the condition, and `resume`. The observed pause must be after 2 ms and at most 1 us + 1 ps later. Resumed output must meet the same analytical voltage gate.
- Background pause: request `bg_halt` after observing progress, record last observed time and actual paused time. Require confirmed idle before inspecting vectors/resuming. No virtual-time bound on this asynchronous request is assumed.
- Test circuit reset/rerun and full library reset/reinitialize/reload. Repeated baseline vectors must agree within 1 ps and 1 nV, with no stale circuit samples.
- Exercise an invalid netlist in a separate process, record command/exit callbacks and diagnostics, and attempt recovery. A process crash or timeout is a finding, never successful rejection.
- Observe external-source trial times, sync callback locations and retries, and accepted samples. A dedicated single-step retry experiment may request a local solver retry; this does not establish rollback of committed co-simulation state.
- Run each case in a child process with a 30-second wall-time limit. Do not invoke engine commands from callbacks. Copy borrowed data before returning. Record callback faults rather than allowing exceptions across C interfaces.

Failure of a capability must remain explicit in the report. Numeric tolerances must not be relaxed to hide a failed comparison. This experiment contains no Renode, firmware, GPIO, ADC, GUI, or coupled feedback.

## Run the experiment

Follow the [Windows package setup](../../../docs/development/WINDOWS_BACKENDS.md) first. Renode is not needed for E-01; `check_backend_assets.py --ngspice-only` verifies its three archives and six selected extracted files. CMake and the runner do not download dependencies. Use only trusted experiment fixtures, not arbitrary user netlists.

From the repository root in PowerShell:

```powershell
$spiceRoot = (Resolve-Path build/deps/ngspice/Spice64_dll).Path
cmake -S tests/experiments/ngspice -B build/e01 -A x64 "-DNGSPICE_ROOT=$spiceRoot"
if ($LASTEXITCODE -ne 0) { throw 'E-01 configuration failed' }
cmake --build build/e01 --config Debug
if ($LASTEXITCODE -ne 0) { throw 'E-01 build failed' }
python tests/experiments/ngspice/run.py --output build/e01-results
if ($LASTEXITCODE -ne 0) { throw 'E-01 failed; inspect its logs and summary' }
python tests/experiments/ngspice/run.py --output build/e01-repeat --compare-with build/e01-results
```

Use an explicit Visual Studio generator when multiple installations exist. `--executable` selects a different host build, `--deps` selects the extracted dependency root, and `--cases` selects individual cases. The background case substitutes `.tran 1u 50m 0 1u uic` in the RC fixture so there is time to request a pause. All native API time values are seconds.

The runner writes `summary.json`, per-case `metrics.json`, accepted-vector CSVs, copied callback/source/sync CSVs, and `engine.log`. Reusing an output directory overwrites its evidence files; use a new directory to preserve a previous run. Any failure or timeout returns nonzero. `--compare-with` compares deterministic final vectors; background pause location and its resumed sampling grid are not required to repeat exactly.

Optional chart generation, with matplotlib already available:

```powershell
python tests/experiments/ngspice/plot.py build/e01-results build/e01-results/rc.svg
```

The [published report](../../../docs/experiments/E-01-results.md) and its compact evidence distinguish the supported profile from untested capabilities. No raw traces or downloaded binaries belong in Git.
