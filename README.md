# SimNodus

**Mixed-Signal & Embedded Systems Simulator**

SimNodus is an open-source (MIT) educational desktop simulator that combines
analog circuits, digital logic and real microcontroller firmware in one
continuously running, interactive simulation. Its first target is the
STM32F103C8/Blue Pill, using Renode for the microcontroller, ngspice for the
circuit and a Qt 6 interface.

**Status (2026-10-14): product rebuild, phase 0.** Backend feasibility is proven
by experiments E-01..E-05 (ngspice, Renode/STM32 GPIO/EXTI, GPIO↔RC coupling,
ADC, CubeIDE/GDB debugging). The application is being rebuilt around a clean
QSpice-like editor, continuous interactive simulation and a separate Signal
Analyzer window. Release 0.1 (analog interactive editor) is planned for
2026-10-30; see the [roadmap](docs/planning/ROADMAP.md).

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
                 ngspice (analog)  <-- lockstep -->  Renode (STM32 firmware)
```

## Repository layout

| Path | Content |
|---|---|
| `docs/` | Requirements, architecture, plans, ADRs, experiment reports |
| `tests/experiments/` | Backend experiments E-01..E-05 (reference, Windows) |
| `src/`, `apps/`, `tests/` | Previous bounded product code, moving to `legacy/` in SN-101 |
| `docs/archive/2026-10-legacy/` | Planning documents of the first development line |

All repository documentation, code and comments are in English.

## License and publication

Original SimNodus code and associated documentation are licensed under the [MIT License](LICENSE). Third-party software, firmware, models, fonts, and other assets keep their own terms; see the [licensing policy](docs/development/LICENSING.md).

Created and maintained by **Ricardo Kerschbaumer** ([RicardoKers](https://github.com/RicardoKers)). Source code and documentation are public at [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus). Follow [issues](https://github.com/RicardoKers/SimNodus/issues), [milestones](https://github.com/RicardoKers/SimNodus/milestones), and [checks](https://github.com/RicardoKers/SimNodus/actions). No simulator release is available yet; the first is planned for 2026-10-30.

STM32 and other product names identify third-party technologies, not affiliations. A preliminary GitHub name search found no matching repository before publication; formal trademark and domain clearance remain unverified.
