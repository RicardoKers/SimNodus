# Product vision

Revision 2 — 2026-10-14. First version: [archive](archive/2026-10-legacy/VISION.md).

## Problem

Students need to see how firmware, peripheral configuration, and electrical behavior affect one another. SimNodus should let them assemble analog/digital circuits around STM32, execute real firmware, and inspect signals and mistakes without requiring a physical workbench for every attempt.

The owner has experienced stability and component/subcircuit authoring difficulties in existing tools. Simple reuse and understandable diagnostics are product requirements.

## Desired experience

An instructor prepares a circuit, firmware, and lesson notes. A student opens the project and runs it. The simulation keeps running while the student presses buttons, turns potentiometers and watches LEDs, meters and waveforms react in the circuit and in the microcontroller program. Signals are examined in a separate analyzer window, and STM32CubeIDE can debug the firmware. A circuit function can later become a reusable subcircuit with explicit ports.

The [desktop experience](architecture/DESKTOP_UX.md) is a clean QSpice-like
editor (components left, schematic center, properties right, adjustable widths)
with continuous interactive simulation and a separate Signal Analyzer window.

The baseline should run locally without an account or continuous network access. Offline classroom operation is a proposed requirement to confirm against laboratory constraints. Dependency downloads belong in setup, not silent project loading.

## First scope

Backend feasibility (firmware, GPIO, RC, digital feedback, ADC, debugging) was proven headless in E-01..E-05. The rebuilt application now delivers in two-week releases: analog interactive editor first (0.1), then STM32F103C8 GPIO/EXTI/UART (0.2), ADC/PWM (0.3) and debugging/packaging (0.4); see the [roadmap](planning/ROADMAP.md).

Windows is the classroom platform; the code is portable and also runs on Linux for development. The intended classroom date is February 2027, with the tool usable for lesson preparation from release 0.1.

Do not confuse an external LED circuit with the board's onboard LED. Pin mapping, LED polarity, memory map, clocks, and board wiring must belong to an explicit board profile.

## Success criteria

- A firmware or circuit fault can be observed and explained using signals and virtual time.
- Another person can reproduce a project with its dependencies.
- Components and subcircuits can be authored without changing the kernel.
- Fidelity and peripheral limitations are visible.
- Invalid files and backend failures produce useful errors without losing the edited circuit.

## Outside the initial commitment

PCB design, cloud services, simultaneous collaboration, a large MCU catalog, cycle-accurate emulation, mandatory real-time operation, thermal simulation, and electrical certification. HDL, WASM, node/register inspection, and advanced decoders remain on the roadmap. Whole-circuit reverse debugging requires separate research and is not promised.

MIT permits broad reuse, including commercial and closed-source derivatives under its conditions. Public development does not imply support guarantees or eliminate third-party obligations.
