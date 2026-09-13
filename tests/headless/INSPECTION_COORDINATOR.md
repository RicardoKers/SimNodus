# Extracted final inspection coordinator

The session CLI now delegates final readback, register inspection and post-pair
time confirmation to `FixtureInspection` in
[src/application](../../src/application/fixture_inspection.hpp). See
[ADR 0038](../../docs/decisions/0038-fixture-inspection-coordinator.md).
The [post-pair protocol](INSPECTION_TIME.md) and its full prerequisite chain are
unchanged. The coordinator owns private state and acceptance rules; the CLI owns
engine transports and issues the separate joint commit.

## Reproduction

Use the real-engine command from [post-pair time](INSPECTION_TIME.md), replacing
output paths with fresh `build/sn017/inspection-coordinator-*` directories.
This cycle uses `guarded-01`, the four `<fault>-loss-01` controls and `recovery-01`.
Keep all native flags, steps, lifecycle, pause and guard for successful runs;
fault runs omit those four controls and select the existing fault argument.
Exact inputs and report bodies are retained in the
[evidence](../../docs/experiments/evidence/SN-017-inspection-coordinator-summary.json).

```sh
cmake --build build/sn017/native --config Debug
ctest --test-dir build/sn017/native -C Debug --output-on-failure
python tests/headless/inspection_time_regression.py --runner build/sn017/native/Debug/simnodus_session_contract.exe --output build/sn017/inspection-coordinator-cases-01
python -m unittest discover -s tests/experiments/debugging -p "test_*.py"
```

Select unused output directories when repeating. Existing pipe suites cover all
legacy readback/inspection modes, raw/token errors, pending operations, duplicates,
late replies and shared-budget expiry. The direct `fixture-inspection` CTest
checks independent instances, delegation and commit gating without engines;
real integration is validated separately with Renode/ngspice/GDB.

Preserve all earlier evidence, PDF suppression and startup PID retry. No new
backend or peripheral capability is implied. Host transport and untagged stream
association remain; the fixture coordinator is still protocol-aware.
