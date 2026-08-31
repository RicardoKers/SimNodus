# Dependency inventory

Status: selection/investigation, not an installed or validated simulation stack. Update this file in SN-010 before integration work.

| Dependency | Role | Selection state | Required evidence |
|---|---|---|---|
| C++ compiler | Core and adapters | C++20 baseline; local MSVC installation detected | Exact compiler/SDK and architecture in build report |
| CMake | Build orchestration | Minimum 3.24; local 4.2.3-msvc3 found | Successful configure; reproducible generator settings |
| Python | Repository tools | Minimum 3.10; local 3.14.4 found | Standard library only for current checks |
| ngspice/XSPICE | Analog/mixed-signal backend | Version not selected | Shared library build, API/flags, notices, checksum |
| Renode + external client | MCU backend | Version not selected | Matching executable/client/platform, build/run recipe, licenses |
| ARM toolchain | Firmware ELF | Not selected/validated | Compiler/linker version, startup provenance, reproducible ELF |
| STM32CubeIDE | Student debugging workflow | Not selected/validated | Version and tested launch recipe |
| Qt 6 | Desktop UI | Major version direction only | Exact release/modules/compiler/runtime and distribution review |
| WASM runtime | Future component SDK | Deferred | Capability/security/license evaluation |
| Verilator | Future HDL path | Deferred | Integration ownership, build/runtime and license review |

Do not add a lockfile full of invented or untested versions. During each selection, record source URL, immutable revision, download checksum, local build flags, transitive dependencies, and redistribution status.

No runtime dependencies are vendored or downloaded by the current CMake project. Keep owned experiments in the repository and downloaded/build outputs outside tracked sources.
