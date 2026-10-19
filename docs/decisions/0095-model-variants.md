# ADR 0095: Model variants of basic parts, built in and from the user's folder

Date: 2026-10-18. Status: accepted (owner's request of 2026-10-18, SN-194,
requirement PRJ-07).

## Context

Transistors, MOSFETs and diodes had one fixed SPICE model each. The owner asked
for a choice of real parts in the Properties panel (2N2222, BC547, 1N4148...)
and for a practical way to add more, naming the
[KiCad-Spice-Library](https://github.com/kicad-spice-library/KiCad-Spice-Library)
as an example of available models. That repository declares GPL-3.0 for its
scripts only; every model file keeps the terms of its manufacturer, which
nobody has reviewed. SimNodus is MIT and bundles only material under
compatible, checked terms. Projects must also open the same way on every
classroom PC, and opening a project never executes code.

## Decision

- A component parameter of kind `model` names a SPICE model of a given type
  (NPN, PNP, D, NMOS, PMOS). The NPN, PNP, N-/P-channel MOSFET and diode use it;
  the default is `Generic`, the former fixed model, so existing projects
  simulate exactly as before.
- **Built-in models** are `.model` statements in `*.lib` files of the
  component library (`resources/library/models/basic_models.lib`). They are
  SimNodus's own approximations from typical datasheet values, under MIT,
  labeled as such: not vendor models and not measured fits.
- **The user's models** are `.model` statements in `*.lib` / `*.mod` files of a
  folder (Documents/SimNodus/models; Tools > Open Models Folder creates it with
  a README). The user may paste lines from manufacturer files or the
  KiCad-Spice-Library there; the license of what they copy is theirs to check.
- Choosing a model from the folder **copies its statement into the component**
  (`models` in the project file). The project then needs neither the folder nor
  the same library on another PC; a built-in choice keeps nothing.
- Model text is data. Only `.model` statements of the five types are taken;
  subcircuits, other dot commands and unusual names are refused. Netlists use
  SimNodus's own model names (`BM_<type>_<name>`, `UM_<part>_<parameter>`), so
  user names never clash.
- **Every parameter is checked before it reaches ngspice** (added 2026-10-18
  after the owner's question): a value that is not a number (`IS=abc`,
  `{expr}`) stopped ngspice for the rest of the session, measured, and a name
  that is not a parameter of the type (`FOO=1`) was ignored by ngspice without
  a word. Both are now left out of the model with a report: in the Messages
  panel when the models folder is read (file, line, model, reason), and as a
  pre-run warning for a part whose kept model has such a parameter. Vendor
  fields that only describe the part (`mfg`, `Vceo`, `Iave`, `type`...) are
  left out silently. Names are checked against ngspice's diode, Gummel-Poon and
  MOSFET level 1-3 parameters; for MOSFET levels above 3 only values are checked.

- **Kinds of diodes** (added 2026-10-18): a model's `type=` (as in LTspice
  libraries: silicon, schottky, germanium, zener, led, varactor...) is kept, and
  each diode part offers the models of its kinds (`model_kinds`).

## Alternatives

- **Bundle the KiCad-Spice-Library or vendor models:** the largest choice, but
  the terms of each file are unknown or restrictive; rejected.
- **Edit the component JSON to add models (the owner's first idea):** possible
  for the built-in set, but library files are part of the installation and a
  project would depend on edits made on one PC; the folder plus copy into the
  project is simpler for students.
- **Keep a project-wide model list instead of per component:** fewer copies,
  but every edit would need a second undo path and conflicts between parts with
  the same name; the per-component copy reuses the existing component edits.

## Consequences

- The Properties panel offers a searchable list per part; tooltips show the
  description and where a model comes from.
- A project file may carry `.model` statements; they are validated when read
  into a netlist, and a missing or wrong-type model is reported for the part.
- Subcircuit models (`.subckt`, used by many op-amps, regulators and power
  MOSFETs) are not supported yet; they need a component with matching pins and
  belong to PRJ-06 (subcircuits).

## Revisit criteria

- Students need parts available only as subcircuits.
- A model source with clear, compatible terms becomes available for bundling.
