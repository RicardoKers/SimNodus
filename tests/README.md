# Tests

| Folder | Content | Runs in |
|---|---|---|
| `unit/` | Qt-free unit tests (`sim-unit`) and real-engine tests against ngspice (`engine-ngspice`) | ctest |
| `gui/` | Offscreen Qt Test smoke tests of the windows (`gui-smoke`) and the executable's `--smoke-test` (`gui-app-smoke`) | ctest |
| `spikes/` | Engine spikes S1 (continuous run, SN-103) and S2 (platform comparison, SN-104); short versions run in ctest | ctest, `run_s1.py` |
| `experiments/` | Backend experiments E-01..E-05 of the first development line; reference only, separate workflows | own CMake projects |

Engine tests need an ngspice library: on Windows `python tools/fetch_backends.py`
stages it in `build/deps/ngspice-runtime`; on Linux install the distribution
package (`libngspice0-dev`). Without it the engine tests are not registered,
and the configure step says so.

Real-engine results are evidence; mocks do not count as integration evidence
([QUALITY](../docs/development/QUALITY.md)).
