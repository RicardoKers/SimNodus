# Getting started

Revision 2 — 2026-10-14 (rebuilt application). The previous guide described
the first development line; its commands for the legacy tree are in
[legacy/README.md](../../legacy/README.md).

## Requirements

| Tool | Windows (classroom platform) | Linux (development, CI) |
|---|---|---|
| C++20 compiler | MSVC 19.4x or newer (Visual Studio 2022/2026 Build Tools) | GCC 13 or newer |
| CMake / Ninja | CMake 3.24+, Ninja (both ship with Visual Studio) | `cmake`, `ninja-build` |
| Qt | Qt 6.5+ MSVC 64-bit kit (owner's machine: Qt 6.11.1 `msvc2022_64`) | Qt 6.4+ (`qt6-base-dev`, `qt6-qpa-plugins`) |
| ngspice | ngspice-47 DLL, fetched and verified by `tools/fetch_backends.py` | distribution library (`libngspice0-dev`) |
| Python | 3.10+, standard library only | 3.10+ |

Renode 1.16.1 is needed only from phase 2: `python tools/fetch_backends.py --renode`.

## Windows

From a *Developer PowerShell for VS* (or after running `vcvars64.bat`):

```powershell
python tools/fetch_backends.py          # downloads, checks SHA-256, stages build/deps/ngspice-runtime
cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64
cmake --build build/dev
ctest --test-dir build/dev --output-on-failure
python tools/check_repository.py
python tools/package_windows.py --build-dir build/dev   # writes dist/SimNodus-<version>-windows-x64.zip
```

To start the application from the build tree, put the Qt `bin` folder on
`PATH` (or use the package, which contains everything). The packaged
`simnodus.exe --check-engine` loads the bundled engine and solves a reference
circuit; `--smoke-test` opens the windows briefly and checks them.

## Linux

```sh
sudo apt-get install ninja-build qt6-base-dev qt6-qpa-plugins libgl1-mesa-dev libxkbcommon-dev libngspice0-dev
cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/dev
ctest --test-dir build/dev --output-on-failure
```

## Options

| CMake option | Default | Meaning |
|---|---|---|
| `SIMNODUS_BUILD_APP` | ON | Build the Qt application and GUI tests |
| `SIMNODUS_BUILD_SPIKES` | ON | Build the engine spike programs |
| `SIMNODUS_NGSPICE_LIBRARY` | auto | ngspice library for engine tests; empty skips them |
| `SIMNODUS_NGSPICE_RUNTIME_DIR` | `build/deps/ngspice-runtime` | Windows: engine files bundled by `cmake --install` |
| `SIMNODUS_VERSION_SUFFIX` | `dev` | Pre-release label of the version |

## Engine spikes

```sh
python tests/spikes/run_s1.py --exe build/dev/tests/spikes/s1_continuous --library <ngspice library> \
    --scripts resources/ngspice --output build/spikes/s1   # add --quick for the short matrix
```

Results and their interpretation: [S1](../experiments/S1-continuous-run.md),
[S2](../experiments/S2-ngspice-platforms.md).

## Continuous integration

[ci.yml](../../.github/workflows/ci.yml) builds and tests on Ubuntu 24.04 and
Windows, runs the S1 quick matrix on both, compares S2 results across the two
platforms, and uploads the Windows zip as the `simnodus-windows-x64` artifact.
The E-01..E-05 experiment workflows are unchanged and run only when their
files change.
