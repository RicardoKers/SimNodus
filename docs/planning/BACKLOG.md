# Backlog

Revision 2 — 2026-10-14. Tasks for the rebuilt product. IDs from SN-100 upward;
the previous backlog (SN-001..SN-045) is archived in
[archive/2026-10-legacy/BACKLOG.md](../archive/2026-10-legacy/BACKLOG.md).
Status: `todo`, `doing`, `done`. A task is `done` only when its check passes.

## Phase 0 — Reset (until 2026-10-16)

| ID | Task | Check | Status |
|---|---|---|---|
| SN-100 | New requirements, UI spec, architecture, roadmap, ADRs 0079–0081, archive of legacy planning | Owner review; repository checker passes | done |
| SN-101 | Move previous product code (`src/`, `apps/`, related tests) to `legacy/`, keep backend experiments; simplified CMake and CI | Clean configure/build on Linux; CI green | done |
| SN-102 | Application skeleton: main window with three resizable/collapsible columns, analyzer window, settings persistence, empty canvas with grid/zoom/pan | Runs on Linux (offscreen smoke test) and Windows | done |
| SN-103 | Spike S1 — continuous ngspice run: pacing in the data callback, external sources for controls, memory growth and segmented restart with initial conditions | Report with measured memory/speed; recommendation recorded in ADR 0080 | done |
| SN-104 | Spike S2 — ngspice on Windows: runtime loading of the bundled DLL, version 47 vs Linux distribution version | Same netlist gives same result on both | done |

## Phase 1 — Analog interactive editor, release 0.1 (until 2026-10-30)

| ID | Task | Requirements | Status |
|---|---|---|---|
| SN-110 | Document model with stable IDs and edit commands; unit tests | UI-08, PRJ-01 | done |
| SN-111 | Library format, loader and component set A symbols/templates | PRJ-04, SIM-06, UI-04 | done |
| SN-112 | Canvas: place, select, move, rotate, mirror, delete, copy/paste, undo/redo | UI-05, UI-08, UI-09, UI-11 | done |
| SN-113 | Wiring with orthogonal routing, junctions, attachment on move, net labels, ground | UI-06, UI-07 | done |
| SN-114 | Properties panel with engineering notation and validation | UI-10 | done |
| SN-115 | `.snx` save/open, recent files, backup, examples menu | PRJ-01..03, UI-15 | done |
| SN-116 | Connectivity, pre-run checks and SPICE generation with golden tests | SIM-05 | done |
| SN-117 | ngspice engine wrapper (runtime load, callbacks, errors) | SIM-04 | done |
| SN-118 | Interactive run: pacing, speed factor, controls, bounded memory, status | SIM-01, SIM-02, INT-01, UI-13 | done |
| SN-119 | Interactive components and live rendering (switches, button, pot, LED, meters, tooltips, live values) | INT-02..05 | done |
| SN-120 | Signal Analyzer scope: channels, roll/trigger, zoom, cursors, measurements, CSV/PNG | AN-01..06, AN-09 | done |
| SN-121 | Transient mode and settings dialog | SIM-03 | done |
| SN-122 | Messages panel, diagnostics with locate | UI-12, SIM-05 | done |
| SN-123 | Windows package 0.1 (zip with Qt runtime and ngspice), quick-start guide, 4–6 analog examples | NF-02 | done |

## Phase 2 — MCU GPIO, release 0.2 (until 2026-11-13)

| ID | Task | Requirements | Status |
|---|---|---|---|
| SN-130 | Renode engine wrapper (Linux and Windows, pinned 1.16.1), process lifecycle, time control | MCU-02 | done |
| SN-131 | Blue Pill component, board profile, ELF selection/reload, pin state display | MCU-01, INT-06 | done |
| SN-132 | Coupler: lockstep quantum, GPIO output electrical model into ngspice | MCU-02, MCU-03 | todo |
| SN-133 | GPIO input with Schmitt thresholds and EXTI | MCU-04 | todo |
| SN-134 | UART terminal | MCU-05, AN-08 | todo |
| SN-135 | Logic lanes in the analyzer | AN-07 | todo |
| SN-136 | Lesson 1 and 2 example projects with firmware source | — | todo |

## Phase 3 — ADC, PWM, components B, release 0.3 (until 2026-11-27)

| ID | Task | Requirements | Status |
|---|---|---|---|
| SN-140 | ADC through the owned F103 model with circuit voltages | MCU-06 | todo |
| SN-141 | Timer/PWM audit of Renode STM32F1 model and support | MCU-07 | todo |
| SN-142 | Component set B | SIM-07 | todo |
| SN-143 | Lesson 3 example | — | todo |

## Phase 4 — Debugging and packaging, release 0.4 (until 2026-12-11)

| ID | Task | Requirements | Status |
|---|---|---|---|
| SN-150 | CubeIDE/GDB debugging with the cancellable Renode build | MCU-08 | todo |
| SN-151 | Engine in a child process | SIM-08 | todo |
| SN-152 | Windows installer and clean-PC test | NF-02 | todo |
| SN-153 | pt-BR translation | UI-16 | todo |
| SN-154 | User guide and known limitations | MCU-09 | todo |

## Later (after freeze or after classroom)

| ID | Task |
|---|---|
| SN-160 | Symbol Editor window (PRJ-05) |
| SN-161 | Subcircuits (PRJ-06) |
| SN-162 | Dark theme, ANSI symbol variant |
| SN-163 | Headless CLI over the same session API (ADR 0055 direction) |
| SN-164 | Linux package |
