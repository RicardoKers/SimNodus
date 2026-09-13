# Getting started

## What exists

The repository contains the documentation/build foundation, standalone ngspice and Renode experiments, a working bounded GPIO/RC/EXTI coupling experiment, and a focused STM32F103 ADC experiment. There is no production simulation kernel or Qt application yet. A successful foundation check or bounded backend experiment does not establish a complete simulator.

## Documentation check

Python 3.10 or newer is sufficient; no Python packages are required.

```sh
python tools/check_repository.py
```

The checker validates required files, UTF-8/text hygiene, fenced Markdown balance, local Markdown file links, and JSON syntax. It does not validate external URLs, Markdown anchors, YAML semantics, English grammar, security, or simulation behavior.

## CMake foundation

Use CMake 3.24 or newer, Python 3.10+, and a C++20-capable compiler. No third-party simulation dependencies are downloaded by configuration.

From a Windows terminal with a working compiler installation, or a configured Linux development environment:

```sh
cmake -S . -B build/bootstrap
cmake --build build/bootstrap --target check-repository --config Debug
```

CMake discovers the local generator/compiler. On Windows, use a Visual Studio C++ installation or its developer shell; do not assume `cl` is on an ordinary terminal's PATH. Linux compiler detection is a portability check, not a supported Linux application release.

If the intended Python is not selected, pass `-DPython3_EXECUTABLE=/path/to/python` using the path appropriate to your machine. Keep personal overrides outside committed files.

The optional `bootstrap` configure/build presets run the same steps. Build output belongs under `build/`.

Repeated Windows experiments can accumulate extracted Renode runtimes. Follow
the [safe cache cleanup procedure](BUILD_STORAGE.md) after stopping the runners.
Do not delete all of `build/` while unfinished prototype sources or unrecorded
evidence remain there.

## First development task

SN-010 through SN-017 and SN-019 are complete for their declared bounded profiles.
[ADR 0043](../decisions/0043-bounded-headless-extraction-acceptance.md) and the
[SN-017 audit](../experiments/SN-017-acceptance.md) define the native/Python headless
composition and its remaining host responsibilities. Next is SN-018: establish
reproducibility/performance measurements for the same fixed profile. Follow
[CURRENT](../planning/CURRENT.md) and retain real-engine evidence. Qt is not needed.
General unpaced debugging and production readiness remain unapproved.

Do not download arbitrary model packs or install all future dependencies in advance. Choose a dependency manager only after assessing the small backend builds; pin reproducible versions in the eventual lockfile.

## Local baseline observed on 2026-08-31

The [dependency inventory](../research/DEPENDENCIES.md) records the exact host, compiler, SDK, Renode/ngspice packages, and existing CubeIDE ARM tools. Startup, [E-01 RC/lifecycle](../experiments/E-01-results.md), [E-02 firmware/I/O](../experiments/E-02-results.md), restricted [E-03 coupling](../experiments/E-03-results.md), and focused [E-04 ADC](../experiments/E-04-results.md) passed with documented limitations. Commands missing from PATH do not establish that software is absent from the computer.

Machine-specific paths are intentionally not committed. E-05 must predeclare
debugger ownership, effective stop-state evidence, supported commands, overshoot,
disconnect, reset, backend failure, and timeout gates before measuring GDB or
CubeIDE behavior.
