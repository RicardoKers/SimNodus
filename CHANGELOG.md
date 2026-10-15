# Changelog

All entries describe actual repository changes, not promised simulator capabilities.

## Unreleased

### Added (2026-10-15, phase 2: SN-134)

- UART terminal in the Signal Analyzer ([ADR 0086](docs/decisions/0086-uart-terminal.md)): choose a board's USART1–3, read what the firmware transmits, send text with a selectable line ending, optional time stamps per line. New `UART` command of the loopback control protocol (server and client); USART1–3 in the platform; the board's `uarts` list in the library.
- Example 09 with `firmware/uart_echo` (USART1 at 115200 baud on a real board): "tick N" every second, echo, `1` / `0` switch the LED on PA0.

### Added (2026-10-15, phase 2: SN-133)

- GPIO inputs read the circuit ([ADR 0085](docs/decisions/0085-gpio-inputs-and-quantum.md)): Schmitt thresholds 1.16 V / 1.83 V on each pin voltage; levels reach the firmware within the coupling quantum, new project setting `mcu_quantum` (default 1 ms, 0.1–20 ms) in Simulation Settings. EXTI lines 0–15 from ports A–C routed to the NVIC as on the chip; a push button on the schematic fires the firmware's interrupt (measured response 1.1 ms).
- Own STM32F1 GPIO model `resources/renode/SimNodusGpio.cs` (from Renode's MIT model): correct CRL/CRH reset value and a real ODR register, so internal pull-up/down work; board `extensions`, `ground_pin` and input thresholds in `pin_model`.
- Renode skips the CPU's idle time (`SetGlobalAdvanceImmediately`): idle firmware runs 2–4x faster with unchanged timing.
- Example 08 with `firmware/button_irq` (EXTI1 toggles an LED at each press); input pins show their level on the board symbol.

### Added (2026-10-15, phase 2: SN-132)

- GPIO outputs drive the circuit ([ADR 0084](docs/decisions/0084-gpio-output-coupling.md)): each Blue Pill GPIO pin is a behavioral source set from the firmware's pin history (push-pull 3.3 V / 0 V behind 40 Ω, open drain, 40 kΩ pull-up/down for inputs); the circuit never runs ahead of the firmware. New template token `{ext:NAME}` and board `pin_model`.
- Example 07 drives a red LED through 330 Ω from PA0 (2 Hz, about 3.8 mA) with V(PA0) and the LED current in the analyzer. Test: blink firmware into 1 kΩ gives 3.1731 V and edges within 0.1 ms of 250/500/750/1000 ms.

### Added (2026-10-15, phase 2: SN-131)

- Blue Pill (STM32F103C8) component (`resources/library/mcu/bluepill.json`): the 40 header pins in board order, 3.3 V regulator, PC13 LED and reset button; board profile as library data (`mcu` section, [format](docs/architecture/COMPONENT_LIBRARY.md)). File parameters (`"kind": "file"`) with Browse and Reload in Properties; paths are stored relative to the project and rewritten when the project is saved elsewhere; example copies resolve them against their source folder.
- Interactive runs with a board start Renode first and keep firmware and circuit within 20 ms of simulated time in both directions ([ADR 0083](docs/decisions/0083-microcontroller-lockstep.md)). While running, each GPIO pin shows output high/low, alternate function or input; the PC13 LED lights; clicking RESET restarts the firmware.
- Example `07_Blue_Pill_blink.snx` with `examples/firmware/blink` (C without libraries, committed ELF, `tools/build_firmware.py`, compiled with the GNU Tools for STM32 14.3.rel1 of STM32CubeIDE); the platform description now has GPIO ports B and C.
- `--screenshot-delay` for checks of runs that start slowly.
- Measured Renode deviations recorded in ADR 0083: SysTick 125 ppm fast (RVR instead of RVR + 1 cycles), GPIO configuration reads as analog after reset.

### Added (2026-10-14, phase 2)

- The Windows package contains the microcontroller engine: the unmodified Renode 1.16.1 portable bundle in `renode/` (license texts in `renode/licenses/`, reviewed in ADR 0082) and the support files in `mcu/`. New `simnodus.exe --check-mcu-engine` starts Renode, runs 1 ms of virtual time and stops it; the package script runs it. The zip grows to about 137 MB.
- SN-130: microcontroller engine `src/mcu` ([ADR 0082](docs/decisions/0082-renode-engine-integration.md)). `RenodeEngine` starts one Renode 1.16.1 process per run (Windows Job Object, Linux parent-death signal), loads the loopback-only control server and the STM32F103C8 platform from a per-run folder, and offers exact virtual-time `run_for`, GPIO set/get, GPIO change events with virtual timestamps, 32-bit bus access, and `quit` with a kill fallback. Startup failures report the last lines of the Renode log.
- Vendored Renode external control client (`third_party/renode_external_control`, MIT) with a Winsock/POSIX loopback transport; its callback table now belongs to the connection, and long `RunFor` requests get a longer deadline.
- `tools/fetch_backends.py --renode` also fetches the pinned Linux portable Renode; both CI jobs run the new `engine-renode` test against the real Renode (start, exact time steps, GPIO input, CPU-driven GPIO events from a hand-assembled Thumb loop, failed commands, broken platform, killed backend).

## 0.1.0 — 2026-10-14

First release: analog interactive editor for Windows (portable zip; Linux builds from source).

### Added (2026-10-14, after the owner's retest)

- Help > Keyboard Shortcuts (F1): the shortcut list is read from the menu actions, so it cannot drift from the real keys, followed by the canvas keys and mouse gestures. File > Exit now has Ctrl+Q (Qt's standard Quit key has no binding on Windows). GUI test `shortcut_list_follows_the_menus`.

### Added (2026-10-14, 0.1 polish)

- Crash recovery (NF-04): unsaved changes are written to a recovery file next to the settings every minute; a clean exit or a save removes it, and the next start after a crash offers to restore it.
- Net names on hover while editing (UI-07); differential voltage probes by Alt+click in the Probe tool (AN-02).
- The approximations of interactive mode are stated in the status bar tooltip and as the first line of exported CSV files (SIM-09).

### Fixed (2026-10-14, owner test of the 0.1 build)

- Two more places could delete a widget inside its own signal: the recent-files menu (opening a recent file) and the analyzer's channel scale and visibility controls. Both now rebuild after the event.

- An open switch or push button leaked current (1 GOhm off resistance), so a capacitor behind it slowly charged (0.9 V in 20 s in a 5 V circuit). Off resistance is now 1e15 Ohm; the voltmeter input is 1 GOhm so it does not form a divider with an open switch. Regression test: `session: an open push button does not charge a capacitor` (now about 1 uV after 20 s).
- The application could crash when leaving a value field in the Properties panel by clicking the schematic: applying the value rebuilt the form and deleted the field inside its own focus-out handler. The form is now rebuilt after the event, and old editors are deleted later. Regression test: `leaving_a_value_field_by_clicking_the_schematic_applies_it` (crashed 2 of 3 runs before, 0 of 20 after).

### Added (2026-10-14, phase 1)

- SN-110: Qt-free document model (`src/model`) with stable 64-bit IDs, components, wires, net labels, probes and simulation settings; engineering-notation parser/formatter (`4k7`, `100n`, `1meg`, units); undoable edits computed as the difference between two document states.
- SN-111 (library part): JSON component library format and loader with template validation (`src/library`, [format](docs/architecture/COMPONENT_LIBRARY.md)); the 20 types of component set A in `resources/library`.
- SN-115 (file part): `.snx` project format with atomic save and `.bak` backup, version check and clear errors (`src/io`, [format](docs/architecture/PROJECT_FORMAT.md)).
- Editor operations (`src/editing`): place with automatic designators, orthogonal wires, move/rotate/mirror with attached wires that stay orthogonal, delete, parameters, designators, text offsets, copy/paste with new IDs.
- nlohmann/json 3.11.3 vendored in `third_party/` (MIT).
- SN-116: connectivity from geometry (T junctions, labels, ground, junction dots, open ends), SPICE generation from library templates with control sources for interactive parts, phase-continuous periodic sources (`t0`), and pre-run checks: no ground, no DC path to ground, voltage-source/inductor loops, shorted sources, invalid or out-of-range values, duplicate designators, unknown types, unconnected pins, open wire ends, invalid probes.
- SN-117/118/121 (engine side): `SimulationSession` runs interactive simulations in 1 s segments restarted with capacitor voltages and inductor currents as initial conditions, paced to wall clock x speed (live speed changes, pause/resume, prompt stop), with controls through external sources and bounded ring buffers; transient mode keeps the complete result. Engine failures are explained in plain language. One process-wide engine host shared by the engine check and the session. Tests against the real engine: button charging a capacitor, continuity across segments, pulse phase across segments, 1x pacing, pause, stop, transient accuracy.
- The LED measures its current with a series 0 V source (live brightness).
- Current probes: the netlist inserts a 0 V source in series with a probed pin, giving the exact current into any component pin.
- SN-112/113: schematic editing on the canvas: placement with a ghost symbol (rotate/mirror before placing, repeat until Esc, drag from the tree), selection (click, Ctrl, rubber band), dragging with attached wires that stay orthogonal (one undo step), arrow-key moves, rotate, mirror, delete, cut/copy/paste, select all, movable designator and value texts, orthogonal wire tool (bends, corner toggle, finish on pins/wires), net labels, ground, junction dots and open-end markers. Symbols are drawn from the library data.
- SN-114: Properties panel with engineering-notation validation, apply on Enter/focus-out as one undoable change, common parameters of a multi-selection, designator and label renaming, live values (pin voltages, V, I, P).
- SN-115: New, Open, Save, Save As, recent files, unsaved-changes prompt, examples as untitled copies, project file on the command line.
- SN-118/119: run controls (Run/Pause/Stop/Reset, live speed factor), status bar with simulated time and achieved real-time factor; interactive switches, push buttons (held while the mouse is down) and potentiometers (drag or wheel); LED glow from current, meter readouts on symbols, voltage tooltips, optional wire coloring by voltage; structural edits ask to stop the run first.
- SN-120: Signal Analyzer scope with probe channels, roll and triggered modes (source, level, slope, auto/normal/single), per-channel scale and autoscale, cursors with Δt and values, min/max/mean/RMS/p-p/frequency/duty, CSV and PNG export, transient records with zoom and pan.
- SN-121: Simulation Settings dialog (interactive or transient, steps, segment length).
- SN-122: pre-run diagnostics in the Messages panel; double-click selects and centers the related items; engine failures explained in plain language.
- SN-123 (part): six example projects, quick-start guide (`docs/user/QUICK_START.md`), package with library, examples and guide.
- SN-111: every component of set A is simulated with the real engine in `engine-ngspice` (divider, battery, current source, potentiometer, all switches and buttons, diode, LED colors, NPN, NMOS, instruments, RL/RC transient, pulse and sine generators).

### Added (2026-10-14, phase 0)

- SN-102: Qt 6 Widgets application skeleton `apps/simnodus`: Circuit Editor with component tree (search, preview), schematic canvas (dot grid, zoom at cursor, middle-button and Space+drag pan, fit) and properties columns in adjustable splitters; collapsible panels that reopen at their previous size (Ctrl+1/2/3, F11 focus canvas); collapsible messages panel; status bar; full menu/toolbar with planned actions disabled; separate Signal Analyzer window with scope graticule, timebase and Logic/UART placeholders; window/panel/analyzer settings persisted and restored only onto an available monitor; light Fusion theme; all strings through `tr()`.
- `src/sim`: run-time loading of ngspice (Windows DLL and Linux shared library) with callbacks, external sources and background runs; explicit engine check (Tools > Check Simulation Engine, `--check-engine`); bounded sample ring; process memory query.
- SN-103 spike S1 (continuous run) and SN-104 spike S2 (Windows/Linux ngspice comparison) with reports, reference netlists, the committed Windows ngspice-47 reference result and ADR 0080 recommendation.
- Tests: Qt-free unit tests, real-engine tests, S1/S2 checks and offscreen GUI tests in ctest.
- Tools: `fetch_backends.py` (download, SHA-256 check and staging of the pinned ngspice runtime), `package_windows.py` (portable zip with Qt runtime, ngspice, MSVC runtime, licenses; checks the packaged executable).
- CI workflow building and testing on Ubuntu 24.04 and Windows, uploading the Windows zip.

### Changed (2026-10-14, phase 0)

- SN-101: previous product code (`src/`, `apps/`, `tests/headless`, `tests/schema`, `tests/resources`, root CMake project, editor demo tool) moved to `legacy/`; it still builds on its own (`cmake -S legacy`). Documentation links and experiment scripts point to the new locations. The root CMake project builds only the rebuilt application.

### Changed (2026-10-14, product rebuild)

- Revised requirements (revision 2), QSpice-like desktop UI design with continuous interactive simulation and a separate Signal Analyzer, new product architecture, roadmap with two-week releases (0.1 on 2026-10-30), backlog restarted at SN-100, risks and questions.
- ADR 0079 (product rebuild, lightweight process), ADR 0080 (continuous interactive simulation), ADR 0081 (desktop UI technology).
- Planning documents of the first development line moved to `docs/archive/2026-10-legacy/`; `NEXT_CONVERSATION.md` retired.
- Local SN-045 documentation (automation-first direction, ADR 0055) integrated from the handoff snapshot.

### Added

- E-04 focused STM32F103 ADC extension, owned firmware and native verifier, pinned Renode/ST source audit, repeated 363-conversion local profile, compact evidence, result report, ADR 0012, and Windows workflow.
- Verified integer-microvolt input, one 12-bit quantization, static/boundary/saturation points, two-channel mapping, 101-point ramp, all eight fixed-clock timing selections, start-time sampling, and adverse cases.
- E-03 real Renode/ngspice GPIO/RC/EXTI coupling host, owned firmware, 54-case repeated matrix, compact evidence, result report, ADR 0011, and Windows workflow.
- Measured known-schedule replay and bounded sampled feedback at 1000 us, 100 us, and 20 us, including boundary, pulse, late-input, forced-failure, and fresh-recovery cases.
- SN-013 temporal capability profile and ADR 0010: checked time conversion, measured backend semantics, supported replay, explicitly approximate sampled exchange, unsupported live-feedback operations, and predeclared E-03 gates.
- Owned STM32F103C8 firmware, offline exact-memory profile, real GPIO/input/EXTI experiment, coverage audit, evidence report, ADR 0009, and repeated Windows workflow.
- Native Windows Renode control/time experiment, generated loopback-only server, 20-case suite, pinned source provenance, and a dedicated Windows workflow.
- SN-019 report and ADR 0008: measured timing/reconnection, bounded failures, safe error ownership, and explicit network exposure restrictions; E-02 is now ready.
- Real ngspice RC/lifecycle experiment with eight cases, three recorded local runs, analytical checks, a reference chart, and an opt-in Windows CI workflow.
- ADR 0007 records measured trial/callback, pause, reset, error, and worker-lifetime semantics; no coupled simulation is claimed.
- Pinned Windows backend packages, SHA-256 verifier, C++20 ngspice startup probe, reproducible setup guide, and SN-010 execution report.
- ADR 0006 and SN-019 for the verified native Windows Renode client incompatibility; E-02 external control requires that adaptation.
- English product requirements, architecture, backend/time contracts, and component/subcircuit design.
- Architecture decisions and a Windows-first roadmap targeting classroom use in February 2027.
- Backlog, risk register, open questions, and reproducible experiment specifications.
- MIT license for original project material and third-party review guidance.
- Development scaffolding, repository checks, and GitHub issue/PR/CI templates.
- Public repository under RicardoKers, with Ricardo Kerschbaumer as the initial author.
- Nine initial issues, four dated milestones, private vulnerability reporting, and main-branch protection requiring Windows/Ubuntu checks.

### Not implemented

Production simulation kernel/adapters, general causal feedback, electrical ADC coupling, complete STM32F103 ADC support, circuit editor, project loader, component runtime, and synchronized debugging.
