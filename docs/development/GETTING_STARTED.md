# Getting started

## What exists

The repository contains the documentation/build foundation, a C++ ngspice dependency probe, and a working standalone RC/lifecycle experiment. There is no production simulation kernel, coupled firmware example, or Qt application yet. A successful foundation check or bounded backend experiment does not establish a complete simulator.

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

## First development task

SN-010 and SN-011 are complete. Follow the [Windows backend recipe](WINDOWS_BACKENDS.md) and [E-01 commands](../../tests/experiments/ngspice/README.md) to reproduce their checks. Next, SN-019 must adapt the native Windows Renode client before E-02 external-control acceptance. Qt is not needed for these experiments.

Do not download arbitrary model packs or install all future dependencies in advance. Choose a dependency manager only after assessing the small backend builds; pin reproducible versions in the eventual lockfile.

## Local baseline observed on 2026-08-31

The [dependency inventory](../research/DEPENDENCIES.md) records the exact host, compiler, SDK, Renode/ngspice packages, and existing CubeIDE ARM tools. Startup and [E-01 RC/lifecycle checks](../experiments/E-01-results.md) passed with documented limitations; firmware and coupled behavior remain untested. Commands missing from PATH do not establish that software is absent from the computer.

Machine-specific paths are intentionally not committed.
