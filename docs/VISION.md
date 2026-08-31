# Product vision

## Problem

Students need to see how firmware, peripheral configuration, and electrical behavior affect one another. SimNodus should let them assemble analog/digital circuits around STM32, execute real firmware, and inspect signals and mistakes without requiring a physical workbench for every attempt.

The owner has experienced stability and component/subcircuit authoring difficulties in existing tools. Simple reuse and understandable diagnostics are product requirements.

## Desired experience

An instructor prepares a circuit, firmware, and lesson notes. A student opens the project, runs it, changes controls, observes instruments, and connects STM32CubeIDE for debugging. A circuit function can become a reusable subcircuit with explicit ports.

The baseline should run locally without an account or continuous network access. Offline classroom operation is a proposed requirement to confirm against laboratory constraints. Dependency downloads belong in setup, not silent project loading.

## First scope

One STM32F103C8, a small circuit, and one simulation session. First prove firmware, GPIO, RC, digital feedback, and ADC without an editor. The first graphical teaching release adds minimal editing, essential instruments, and synchronized debugging.

Windows comes first; Linux follows later. The intended classroom date is February 2027. January 2027 is reserved for stabilization, installation checks, and a teaching rehearsal.

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
