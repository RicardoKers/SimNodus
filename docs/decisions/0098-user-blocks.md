# ADR 0098: Blocks drawn by the user are projects placed into projects

Date: 2026-10-19. Status: accepted (owner's choice of 2026-10-18: "Arquivo
próprio" — the block is its own file; requirement PRJ-06, second part).

## Context

Teachers want to draw a stage once (a filter, a divider, a transistor driver)
and use it several times, in the same project and in others. The owner chose
how blocks are stored: a block is an ordinary project file with port markers;
*Place > Block* inserts it as a component with a rectangular symbol made from
the ports; the project keeps a copy, so it opens the same elsewhere; a
double-click opens the block's file.

Two facts of the engine shaped how a block reaches the netlist:

- Interactive runs restart ngspice every segment and carry the voltage of each
  capacitor and the current of each inductor by element name (ADR 0080).
  Inside a `.subckt`, one definition serves every instance, so an instance's
  capacitor could not get its own initial condition; and ngspice's names for
  elements inside subcircuits would have to be reproduced to read them back.
- The editor colors wires by the voltage of the net name it computes itself;
  a netlist that names the nets around a block differently would leave wires
  uncolored.

## Decision

- **Ports.** A *Block port* part (category *Blocks*) has a text parameter
  `name` and a choice `side` (left, right, top, bottom); choosing a side turns
  the port to point that way, as the pin will on the block (added 2026-10-19). Its pin belongs to the
  net named by `name` (`ties_net_parameter`), so a block file simulates on its
  own like any project. Ports with the same name are one pin; a port cannot be
  named GND or 0 (ground is shared anyway); a block cannot hold a
  microcontroller board.
- **Symbol.** The rectangle and pins are made from the ports: left and right
  pins from top to bottom, top and bottom pins from left to right, in the order
  the ports have on the block's schematic; two grid units apart; the block's
  name in the middle. The component type is `block:<name>`, the name being the
  file's name without extension; prefix U.
- **Storage.** The project keeps `"blocks": [{"name", "source", "project"}]`,
  where `project` is the whole block file and `source` its path relative to the
  project's folder. Blocks inside blocks are kept the same way, up to eight
  levels. Projects with blocks are written as format version 2, which 0.3
  refuses with its "newer SimNodus" message; projects without blocks stay at
  version 1. Changing blocks is an undoable edit; copy and paste take the
  blocks of the copied parts along.
- **Netlist: the circuit in place of the block.** Before the netlist is made,
  each block component is replaced by a copy of its circuit, moved beside the
  rest so that no point coincides. Parts get the block's designator in front
  (`U1.R1`, element `R_U1_R1`); labels and buses get a name of their own
  (`U1/OUT`); every net inside without a label gets one (`U1/N002`); nets that
  reach a port join the net on the block's pin through hidden labels that
  connect but never name a net. The nets around the block therefore keep the
  names the editor shows, capacitors inside are carried across segments like
  any other, and the pre-run checks see the whole circuit.
- **Findings** inside a block point at the block's component and start with
  "In block U1:". Switches and potentiometers inside a block keep the state
  saved in the block's file (reported once as information).
- **Editing a block.** A double-click on a block (or *Open* in Properties)
  opens its file in the editor, after the usual question about unsaved
  changes; if the file is gone, the project's copy can be saved as a new file
  first. *Update* in Properties reads the file again; every component of the
  block changes. Opening a project compares each block with its file and offers
  to update the copies that differ.

## Alternatives

- **A `.subckt` per block** (the wrapper of ADR 0097): one definition per block,
  but capacitor states inside would be lost at every segment boundary, and live
  values inside would need ngspice's naming of subcircuit internals.
- **Blocks linked to their files only, no copy:** a project would break when
  sent alone to a student; the copy plus an update offer keeps both.
- **Blocks in a user library folder** (like the models folder): useful later
  for a shared collection; a file per block needs no new folder rules now.

## Consequences

- A block is opaque in the editor: probes and live values reach its pins, not
  its inside; its internal nets are in the netlist (`U1_N002`) for the
  analyzer's vector list.
- Interactive parts inside a block cannot be operated while running.
- Netlist size grows with each instance (no sharing); classroom circuits stay
  far below the sizes where this matters.
- A block renamed on disk keeps its old name in the project until it is placed
  again.

## Revisit criteria

- Students need to watch signals inside a block (open a block "view" of the
  running circuit).
- Shared block collections are asked for (a blocks folder like the models
  folder).
