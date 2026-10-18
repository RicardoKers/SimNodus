# ADR 0096: Buses are drawings; splitters join their bits by name

Date: 2026-10-18. Status: accepted (owner's request of 2026-10-18, SN-196,
requirement UI-19).

## Context

Larger, mostly digital circuits need buses: one thick line for several wires,
with splitters (distributors) that take single signals in and out, as in
hneemann's *Digital*. The owner asked for a visual aid only: the netlist must
stay the same as with individual wires.

## Decision

- A **bus** is an element of its own: an orthogonal polyline with a name
  (`D`). It is drawn thick and connects nothing by itself.
- A **splitter** is an element with a bus point and `count` bit pins (bits
  `first` .. `first + count - 1`, two grid units apart). Each bit pin acts as a
  **net label** named after the bus and the bit: bit 3 of bus `D` is the net
  `D3`. Splitters of the same bus therefore join bits with the same number,
  and a label `D3` elsewhere joins them too, exactly as labels do today.
- Renaming a bus renames the splitters whose bus point lies on it, so their
  bits keep matching the bus; a splitter can also be edited on its own
  (bus name, first bit, number of bits).
- Netlists, nets and simulation are unchanged: the splitter's bit pins add
  names to nets, nothing else.

## Alternatives

- **Connect through the bus geometry** (a bit joins the bit of every splitter
  on the same drawn bus): closer to *Digital*, but buses would become part of
  connectivity (crossing and touching rules, bus-to-bus joints) for no
  electrical gain; names already give the same result and are what students
  see in KiCad-style schematics.
- **Bus wires that carry real multi-bit values**: needs a digital simulator,
  not SPICE nets; out of scope.

## Consequences

- Two separate buses with the same name join their bits, like two labels with
  the same name; the bus name is shown on the drawing so this stays visible.
- A splitter whose bus point is not on a bus still works by its name.
- Project files gain `buses` and `splitters` arrays; older files load as before.

## Revisit criteria

- Students wire buses with splitters on different buses of the same name by
  mistake: then add a pre-run warning for a splitter not on a bus of its name.
