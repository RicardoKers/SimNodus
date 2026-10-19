# ADR 0097: Vendor subcircuits as part models

Date: 2026-10-18. Status: accepted (owner's priority of 2026-10-18: subcircuits,
vendor `.subckt` models first, then blocks drawn by the user; requirement PRJ-06).

## Context

Most vendor models of op-amps, regulators, Darlingtons and power MOSFETs are
subcircuits (`.subckt NAME ports ... .ends`), not single `.model` statements.
[ADR 0095](0095-model-variants.md) let parts use `.model` statements from the
user's models folder and skipped subcircuits. Real files (National's LM358, TI's
TL072, OP07, a TIP120 Darlington, an AO3400 MOSFET from the KiCad-Spice-Library)
showed what such files contain: names with `/` (`LM358/NS`), models and helper
subcircuits next to the one a part uses, `POLY` sources, JFET and resistor
models, and comments that say which port is which pin. Two measurements shaped
the decision:

- ngspice turns `POLY` sources into the XSPICE code model of `spice2poly.cm`.
  The package did not load it, so LM358 and TL072 were refused; with it loaded
  all five parts simulate (gain-2 amplifier: 2.0037 V, 1.9999 V, 2.0001 V;
  TIP120 1.03 V and AO3400 24 mV at 1 A).
- An error ngspice cannot recover from stopped every later run until SimNodus
  was restarted; vendor syntax can cause such errors.

## Decision

- **Parts that accept subcircuits.** A `model` parameter may list the part's
  pins in the usual port order of vendor files (`"subcircuit"`): op-amp IN+,
  IN-, V+, V-, OUT; NPN/PNP C, B, E; MOSFETs D, G, S; diode A, K. The op-amp
  keeps its behavioral template as the default.
- **Reading.** The models folder gives `.subckt` blocks as well as `.model`
  statements. Each block is made self-contained: the file's `.model`
  statements and `.subckt` blocks it uses (transitively) come with it. A block
  that uses a dot command other than `.model`, `.param`, `.func`, `.subckt` and
  `.ends` (for example `.include`, `.lib`, `.control`, which could read files or
  run commands) is skipped and reported, as is one that uses such a block.
- **Choosing.** The Properties list offers the subcircuits with one port per
  pin of the part; the chosen text is kept in the component (`models`), like a
  `.model` statement, so projects do not depend on the folder. *Pins...* sets
  which part pin each port is, showing the file's comments; the assignment is
  kept as `models["model.pins"]` and cleared when the model changes.
- **Netlist.** The part's template is replaced by one instance of a wrapper
  subcircuit of SimNodus's own name (`UX_<part>_<parameter>`). Inside it, the
  kept text is defined with the used block renamed `SNX`; ngspice keeps nested
  definitions local, so names inside vendor files never clash between parts.
- **Engine.** `spice2poly.cm` from the pinned ngspice 47 archive (same hash
  check as the library) is staged and packaged next to `ngspice.dll` and loaded
  with `codemodel` when the engine starts. When ngspice still reports that it
  cannot recover, it is unloaded and loaded again before the next run (#160).

## Alternatives

- **Rename every name inside the vendor text:** possible, but every element
  kind would need its own parsing; nesting gives the same isolation for free.
- **Validate vendor syntax ourselves:** ngspice's dialect is too large to check
  fully; the dot-command rule keeps files and commands out, and the engine
  reload limits the harm of anything else to one failed run with ngspice's own
  message.
- **A generic "IC" part with any number of pins:** useful later for parts
  without a symbol; it needs a symbol editor (PRJ-05).

## Consequences

- Vendor op-amps, comparators, Darlingtons and power MOSFETs work on the
  existing symbols; three-terminal regulators and references need symbols of
  their own (backlog).
- A wrong pin assignment simulates nonsense without an error: the dialog shows
  the vendor's comments, and the usual order matches most files.
- The models folder, not the package, holds vendor files: SimNodus bundles
  none of them (licenses of each file, ADR 0095).

## Revisit criteria

- Classroom files need dot commands outside the allowed set.
- Blocks drawn by the user (PRJ-06, second part) reuse this wrapper.
