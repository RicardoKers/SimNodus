# Requirements

Revision 2 — 2026-10-14. Supersedes the first requirements baseline, archived in
[archive/2026-10-legacy/REQUIREMENTS.md](archive/2026-10-legacy/REQUIREMENTS.md).
Decision context: [ADR 0079](decisions/0079-product-rebuild-and-lightweight-process.md),
[ADR 0080](decisions/0080-continuous-interactive-simulation.md) and
[ADR 0081](decisions/0081-desktop-ui-technology.md).

Status column: **planned** (not started), **in progress**, **done** (implemented
and verified by automated tests and a visual check; see the
[CHANGELOG](../CHANGELOG.md)). Phase numbers refer to the
[roadmap](planning/ROADMAP.md).

## 1. Users and core use cases

| User | Core use |
|---|---|
| Instructor | Builds lesson circuits, attaches STM32 firmware, prepares example projects and expected observations |
| Student | Opens a project, runs it continuously, presses buttons/moves controls, watches LEDs, meters and waveforms, changes firmware or circuit, debugs with STM32CubeIDE |

The defining experience: **the simulation runs continuously and reacts to the
user.** Pressing a push button on the schematic must visibly change the circuit
and the microcontroller program (for example an interrupt toggles an LED) while
the simulation keeps running, and the signals can be examined in a separate
analysis window.

## 2. Main window (Circuit Editor)

Visual reference: the clean, technical look of QSpice. Three adjustable columns,
an optional bottom message panel and a compact toolbar.

```text
+--------------------------------------------------------------------------+
| File  Edit  View  Place  Simulate  Tools  Window  Help                   |
| [New][Open][Save] | [Undo][Redo] | [Wire][Probe] | [>Run][||][#Stop] 1.00x|
+---------------+-------------------------------------------+--------------+
| COMPONENTS    |                                           | PROPERTIES   |
| [search.....] |                                           | R3  Resistor |
| v Passive     |                                           | Value  [4.7k]|
|    Resistor   |              SCHEMATIC CANVAS             | Tolerance    |
|    Capacitor  |         (dot grid, wires, symbols,        | ...          |
| v Sources     |          live values while running)       |--------------|
| v Switches    |                                           | LIVE VALUES  |
| v Semicond.   |                                           | V = 2.31 V   |
| v Instruments |                                           | I = 0.49 mA  |
| v MCU         |                                           | P = 1.13 mW  |
|---------------|                                           |              |
| PREVIEW       |                                           |              |
| [symbol]      |                                           |              |
+---------------+-------------------------------------------+--------------+
| Messages (collapsible): diagnostics, engine messages, click to locate    |
+--------------------------------------------------------------------------+
| Running | t = 1.284 s | 0.98x real time | Interactive | 0 warnings       |
+--------------------------------------------------------------------------+
```

| ID | Requirement | Phase | Status |
|---|---|---|---|
| UI-01 | Three-column layout: component tree (left), schematic canvas (center), properties (right). Column widths are adjustable by dragging splitters | 1 | done |
| UI-02 | Each side panel can be collapsed and reopened at its previous width (View menu, toolbar and shortcut). Window geometry, panel widths/visibility and the analyzer geometry persist between sessions and are restored only onto an available monitor | 1 | done |
| UI-03 | Clean light theme: near-white canvas with a subtle dot grid, thin dark wires, consistent symbol line weight, restrained colors used only for meaning (selection, probes, live state). High-DPI aware. A dark theme is optional and later | 1 | done |
| UI-04 | Component tree with categories, incremental search and keyboard navigation. A preview pane under the tree shows the selected symbol, pin count and a one-line description before insertion | 1 | done |
| UI-05 | Placement: drag from the tree or activate an item, then click on the canvas. A ghost symbol follows the cursor; rotate and mirror before placing; repeat placement until Esc. Snap to grid. Automatic reference designators (R1, R2, ...) | 1 | done |
| UI-06 | Wiring: wire tool; click a pin or wire to start, click to add orthogonal bends, click a pin/wire to finish. Automatic junction dots, wire-to-wire T connections, dangling-end markers. Moving a component keeps its wires attached | 1 | done |
| UI-07 | Ground symbol and net labels connect by name. Net names are shown on hover and can be assigned explicitly | 1 | done |
| UI-08 | Selection and editing: click, rubber band, Ctrl toggle; move by dragging or arrow keys; rotate, mirror, delete, cut/copy/paste (new designators), select all. Unlimited multi-level Undo/Redo for every document change | 1 | done |
| UI-09 | Navigation: zoom at cursor with the wheel, pan with middle button (and Space+drag), Fit to content, zoom range suitable for both a full circuit and a single pin | 1 | done |
| UI-10 | Properties panel edits the selected instance: designator, value and model parameters with units. Accept engineering notation (`4k7`, `4.7k`, `100n`, `1meg`, `2.2u`), validate inline, apply on Enter or focus-out as one undoable change. Common properties are editable for multi-selection | 1 | done |
| UI-11 | Designator and value text appear next to each symbol and can be moved independently | 1 | done |
| UI-12 | Messages panel (bottom, collapsible) lists diagnostics and engine messages; double-click selects and centers the related item | 1 | done |
| UI-13 | Status bar shows simulation state, simulated time, achieved real-time factor, mode and warning count | 1 | done |
| UI-14 | Keyboard shortcuts for frequent actions (table in [DESKTOP_UX](architecture/DESKTOP_UX.md#keyboard-shortcuts)); all actions also reachable by menu | 1 | done |
| UI-15 | Examples menu opens bundled example projects as untitled copies | 1 | done |
| UI-16 | All user-visible strings go through Qt translation. English is the base language; a Brazilian Portuguese translation is delivered before classroom use | 1 / 4 | in progress |
| UI-17 | Free text notes on the schematic (instructions for the simulation, explanations); they are presentation only and do not change the netlist | later | done (SN-193, 2026-10-18) |
| UI-18 | The potentiometer shows its wiper position on the symbol (a visual mark and a percentage), while editing and while the simulation runs | later | done (SN-195, 2026-10-18) |
| UI-19 | Buses: a thick line that stands for several wires, with splitters (as in hneemann's *Digital*) that take single wires in and out by name or index; presentation only, the netlist is the same as with the individual wires | later | done (SN-196, 2026-10-18; ADR 0096) |

## 3. Interaction while the simulation runs

| ID | Requirement | Phase | Status |
|---|---|---|---|
| INT-01 | Run, Pause, Resume, Stop and Reset are always available from toolbar, menu and shortcuts; the editor never freezes while the engine works | 1 | done |
| INT-02 | Interactive controls work during Run: toggle switch (click), push button (pressed while the mouse or key is held), potentiometer (drag or wheel), adjustable source value. The effect starts at the next engine step | 1 | done |
| INT-03 | Live visual state: LED brightness/color from its current, lamp brightness, switch position, meter readouts on the schematic, optional wire coloring by voltage. Refresh 10–30 Hz, independent of engine speed | 1 | done |
| INT-04 | Selecting an item while running shows its live values (V, I, P and model-specific values) in the Properties panel; hovering a net or pin shows a tooltip with its present voltage | 1 | done |
| INT-05 | Topology changes (add/delete/move/wire) are not applied to a running simulation. Editing the topology asks to stop the simulation first; value changes of non-interactive parts apply after a restart. The document is never lost because of an engine error | 1 | done |
| INT-06 | Microcontroller pins show their logic state/direction on the symbol while running; the on-board LED (PC13) and reset button of the Blue Pill are interactive | 2 | done |

## 4. Signal Analyzer window

A separate top-level window that can be placed on another monitor.

| ID | Requirement | Phase | Status |
|---|---|---|---|
| AN-01 | Opens from toolbar/menu/shortcut as an independent window; closing it does not stop acquisition or delete retained data | 1 | done |
| AN-02 | Probe tool on the canvas: click a net to add a voltage channel (reference ground by default, differential optional), click a component terminal to add a current channel. Probes are colored like their analyzer traces and are saved in the project | 1 | done |
| AN-03 | Oscilloscope view: multiple analog channels, per-channel scale/offset or autoscale, common time base, roll mode during interactive runs, edge trigger (channel, level, slope) with auto/normal/single, Stop/Run of the display without stopping the simulation | 1 | done |
| AN-04 | Time zoom/pan, two cursors with Δt, 1/Δt and values; measurements: min, max, mean, RMS, peak-to-peak, frequency, duty cycle | 1 | done |
| AN-05 | Export visible channels to CSV and the plot to PNG | 1 | done |
| AN-06 | Transient analysis results open in the analyzer with full zoom over the complete dataset | 1 | done |
| AN-07 | Logic analyzer lanes for digital signals and MCU pins in the same time base as analog channels, with transition timestamps | 2 | done |
| AN-08 | UART terminal: receive/send text for each enabled MCU UART, with time stamps optional | 2 | done |
| AN-09 | Retained history is bounded and configurable (for example the last 10 s at full resolution); limits are visible to the user | 1 | done |
| AN-10 | Export selected digital signals (logic lanes and MCU pins chosen by the user, not all of them) to a VCD (Value Change Dump) file for later analysis in tools such as PulseView | later | done (SN-190, 2026-10-18) |

## 5. Simulation

| ID | Requirement | Phase | Status |
|---|---|---|---|
| SIM-01 | **Interactive mode (default):** continuous transient simulation without a fixed end time, paced to real time multiplied by a user speed factor (for example 0.001x to 1x, or maximum speed). If the engine cannot keep up, the achieved factor is shown instead of pretending real time | 1 | done |
| SIM-02 | Unlimited interactive runs use bounded memory in the engine and in acquisition | 1 | done |
| SIM-03 | **Transient mode:** a deterministic `.tran` with stop time and maximum step; reproducible results for the same project and versions | 1 | done |
| SIM-04 | Analog engine: ngspice as a shared library loaded at run time, never during project opening | 1 | done |
| SIM-05 | Pre-run checks with clickable diagnostics: missing ground, floating nets, unconnected pins, voltage-source/inductor loops, shorted sources, invalid values. Convergence failures are reported in plain language with the involved nets/components | 1 | done |
| SIM-06 | Initial component set A: resistor, capacitor, inductor, potentiometer, DC voltage source, DC current source, battery, pulse/square generator, sine generator, ground, SPST switch, SPDT switch, push button (NO/NC), diode, LED (several colors), NPN transistor, N-channel MOSFET, voltmeter, ammeter | 1 | done |
| SIM-07 | Component set B: PNP, P-channel MOSFET, Zener, op-amp (behavioral), lamp, relay, small DC motor (simplified), transformer, logic gates/74HC subset as needed by lessons | 3 | done (logic gates: SN-191) |
| SIM-08 | The engine runs isolated from the editor so that an engine crash cannot crash the editor or lose the document | 4 | planned |
| SIM-09 | Fidelity limits are visible: approximate modes are labeled in the status bar and in exported data | 1 | done |
| SIM-10 | Teaching models: a diode with a fixed forward voltage (0.7 V, 0.3 V) and NPN/PNP transistors with a fixed Vbe, an adjustable beta and a saturation voltage, for simplified classroom examples; the beta is shown on the symbol | later | done (SN-197, owner's request 2026-10-18) |
| SIM-11 | Diode kinds look different on the schematic (Schottky, germanium next to the plain diode, Zener and LED) and each offers the models of its kind; vendor files classify their models with `type=` | later | done (SN-198, owner's request 2026-10-18) |
| SIM-12 | Digital storage parts (latches, flip-flops, counters, registers, memories) from ngspice's XSPICE digital models or SimNodus's own behavioral models, chosen after a measurement of how their state survives the interactive segments | later | planned (SN-199, owner's request 2026-10-18) |

## 6. Microcontroller (STM32F103C8 / Blue Pill)

| ID | Requirement | Phase | Status |
|---|---|---|---|
| MCU-01 | Blue Pill component with its real pin names; properties select the firmware ELF (path stored relative to the project) and offer Reload | 2 | done |
| MCU-02 | Real firmware runs in SimNodus's own STM32 emulator, in lockstep with the analog engine using a coupling window (100 µs, chosen from measurements). MCU-to-circuit timing within the window is labeled approximate | 2 | done |
| MCU-03 | GPIO output drives the circuit through an electrical pin model (push-pull/open-drain, supply 3.3 V, finite output resistance) | 2 | done |
| MCU-04 | GPIO input reads circuit voltages with Schmitt thresholds; EXTI interrupts fire from circuit transitions (pressing a button on the schematic triggers the firmware interrupt) | 2 | done |
| MCU-05 | UART output/input through the analyzer terminal | 2 | done |
| MCU-06 | ADC channels sample circuit voltages (in SimNodus's own emulator, ADR 0090) | 3 | done (ADC with DMA) |
| MCU-07 | Timers with PWM outputs on the pins (in SimNodus's own emulator, ADR 0090) | 3 | done |
| MCU-08 | STM32CubeIDE/GDB debugging: breakpoint, step, continue and reset pause the whole simulation consistently | 4 | planned |
| MCU-09 | Supported peripherals and known limitations are listed in the user guide | 2–4 | in progress |
| MCU-10 | Serial ports drive and read their pins at the configured baud rate: frames visible on the scope and logic lanes, boards can talk through wires | 3 (emulator) | done (SN-175 TX frames and board links, SN-192 RX decoded from the pin) |
| MCU-11 | SimNodus's own STM32 emulator runs the firmware in-process with exact pin timestamps; it is the only microcontroller engine ([ADR 0089](decisions/0089-own-stm32-emulator.md), [ADR 0094](decisions/0094-remove-renode.md)) | 2–4 | done |

## 7. Projects and library

| ID | Requirement | Phase | Status |
|---|---|---|---|
| PRJ-01 | Single-file, versioned, human-readable project (JSON, extension `.snx`) storing components, geometry, wires, labels, probes, simulation settings and relative firmware paths | 1 | done |
| PRJ-02 | New, Open, Save, Save As, recent files, unsaved-change prompt, atomic save and an automatic backup of the previous version | 1 | done |
| PRJ-03 | Opening a project never executes code, loads native libraries or downloads anything | 1 | done |
| PRJ-04 | Component definitions (symbol graphics, pins, parameters, SPICE model template) are data files in a library folder, not hard-coded, so that a symbol editor can be added later | 1 | done |
| PRJ-05 | Symbol editor: separate window for creating and editing symbols and pin layouts, writing the same library format | 5+ | planned |
| PRJ-06 | Reusable subcircuits with explicit ports | after classroom | planned |
| PRJ-07 | Model variants for basic components (diodes, transistors, MOSFETs; not integrated circuits): the properties panel offers a list of SPICE models (for example 2N2222, BC547, 1N4148) and the user can add more by editing a data file next to the component definition, without rebuilding; sources such as the KiCad-Spice-Library are imported only under compatible license terms | later | done (SN-194, 2026-10-18; ADR 0095; subcircuit models wait for PRJ-06) |

## 8. Nonfunctional requirements

| ID | Requirement |
|---|---|
| NF-01 | Windows 10/11 x64 is the classroom platform; the application also builds and runs on Linux for development and CI |
| NF-02 | Offline operation; ngspice and the Qt runtime are bundled by the Windows package; no account or network needed |
| NF-03 | Responsiveness: engine work never on the GUI thread; canvas interaction stays smooth with 200 components; analyzer redraw at least 20 Hz with 8 channels |
| NF-04 | Robustness: engine errors, timeouts and invalid files produce messages, never a crash or document loss |
| NF-05 | Automated tests: model/netlist unit tests, simulation results against analytical references, offscreen GUI smoke tests; CI builds Windows and Linux and publishes a Windows test package |
| NF-06 | Licensing: original material MIT; dependencies used under compatible terms (Qt LGPL dynamic linking, ngspice BSD-3); no GPL-only plotting or widget libraries |
| NF-07 | Repository content in English; conversation with the owner may be in Portuguese |

## 9. Outside the current commitment

PCB design, cloud services, collaboration, cycle-accurate MCU timing, a large MCU
catalog, HDL/Verilog, WASM components, AC/Bode/FFT analysis and protocol decoders
beyond the UART terminal. They may return after the classroom release.
