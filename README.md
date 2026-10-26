# SimNodus

**Mixed-Signal & Embedded Systems Simulator**

SimNodus is an open-source (MIT) educational desktop simulator that combines
analog circuits, digital logic and real microcontroller firmware in one
continuously running, interactive simulation. Its first target is the
STM32F103C8/Blue Pill, using SimNodus's own STM32 emulator for the
microcontroller, ngspice for the circuit and a Qt 6 interface.

**Status (2026-10-25): release 0.10.0.** Draw analog and digital circuits
(89 component types, buses, your own blocks), run them continuously
while clicking switches, holding push buttons and turning potentiometers, and
watch the signals in the Signal Analyzer. A Blue Pill (STM32F103C8) runs your
firmware in SimNodus's own emulator, drives SPI and I2C chips (LED
drivers, an EEPROM, a temperature sensor, a character LCD) and can be
debugged from STM32CubeIDE; its instructions take the real chip's cycles
(calibrated on a Blue Pill).
English and Brazilian Portuguese interface and user guides; simulations run in a separate
engine process. AI agents (Claude Code, Codex) can build and simulate
circuits through `simnodus-cli` and its MCP server, also on screen in the
window ([automation](docs/user/AUTOMATION.md)). Download the Windows installer or the portable zip from the
[releases](https://github.com/RicardoKers/SimNodus/releases) and see the
[quick start](docs/user/QUICK_START.md) and the [user guide](docs/user/USER_GUIDE.md);
plans in the [roadmap](docs/planning/ROADMAP.md). Build instructions:
[getting started](docs/development/GETTING_STARTED.md).

## Start here

| Purpose | Document |
|---|---|
| Resume development | [Current state](docs/planning/CURRENT.md) and [agent guide](AGENTS.md) |
| Product requirements | [Requirements](docs/REQUIREMENTS.md) and [vision](docs/VISION.md) |
| User interface | [Desktop UI design](docs/architecture/DESKTOP_UX.md) |
| Code structure | [Product architecture](docs/architecture/PRODUCT_ARCHITECTURE.md) |
| Plan | [Roadmap](docs/planning/ROADMAP.md) and [backlog](docs/planning/BACKLOG.md) |
| Decisions | [ADR index](docs/decisions/README.md) |
| Backend evidence | [Experiments](docs/experiments/README.md) |
| All documentation | [Documentation index](docs/README.md) |

## Planned experience

```text
+------------+-------------------------------+------------+     +------------------+
| Components |  schematic canvas             | Properties |     | Signal Analyzer  |
|  (tree,    |  press buttons, turn pots,    |  values,   | --> |  scope, logic,   |
|  preview)  |  LEDs and meters react live   |  live V/I  |     |  UART, cursors   |
+------------+-------------------------------+------------+     +------------------+
                 ngspice (analog)  <-- lockstep -->  own emulator (STM32 firmware)
```

## Repository layout

| Path | Content |
|---|---|
| `apps/simnodus/` | Qt 6 Widgets application (editor, Signal Analyzer) |
| `src/` | Qt-free libraries (engine runtime now; model, library, netlist, I/O next) |
| `tests/` | Unit, engine, GUI and spike tests; backend experiments E-01..E-05 in `tests/experiments/` |
| `resources/` | Owned ngspice initialization, package README and license notices |
| `tools/` | Repository checker, backend fetch/verify, Windows packaging |
| `docs/` | Requirements, architecture, plans, ADRs, experiment reports |
| `legacy/` | Product code of the first development line, not built by default |
| `docs/archive/2026-10-legacy/` | Planning documents of the first development line |

All repository documentation, code and comments are in English.

## License and publication

Original SimNodus code and associated documentation are licensed under the [MIT License](LICENSE). Third-party software, firmware, models, fonts, and other assets keep their own terms; see the [licensing policy](docs/development/LICENSING.md).

Created and maintained by **Ricardo Kerschbaumer** ([RicardoKers](https://github.com/RicardoKers)). Source code and documentation are public at [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus). Follow [issues](https://github.com/RicardoKers/SimNodus/issues), [milestones](https://github.com/RicardoKers/SimNodus/milestones), and [checks](https://github.com/RicardoKers/SimNodus/actions). Releases are on the [releases page](https://github.com/RicardoKers/SimNodus/releases); development builds of the Windows zip are attached to each CI run.

STM32 and other product names identify third-party technologies, not affiliations. A preliminary GitHub name search found no matching repository before publication; formal trademark and domain clearance remain unverified.
