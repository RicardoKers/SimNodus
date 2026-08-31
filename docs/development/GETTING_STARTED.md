# Getting started

## What exists

The repository is a documentation and build foundation. There are no C++ simulation sources, firmware examples, Qt application, or linked backends yet. Do not interpret a successful configure/check as a working simulator.

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

Execute SN-010, then E-01/E-02. Record the exact Renode client/executable/platform combination and ngspice shared-library build. Audit compiler architecture and native runtime compatibility. Qt is not needed for the initial experiments.

Do not download arbitrary model packs or install all future dependencies in advance. Choose a dependency manager only after assessing the small backend builds; pin reproducible versions in the eventual lockfile.

## Local baseline observed on 2026-08-31

Git 2.53.0.windows.1, Python 3.14.4, CMake 4.2.3-msvc3, and a Visual Studio Build Tools installation were found. Renode, ngspice, and an ARM compiler were not validated. Commands missing from PATH do not establish that software is absent from the computer.

Machine-specific paths are intentionally not committed.
