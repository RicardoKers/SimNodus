# Project file format (.snx)

Revision 3 — 2026-10-19 (SN-110, SN-115; PRJ-01..PRJ-03; blocks PRJ-06). Implementation:
[project_file](../../src/io/project_file.hpp). The project-directory format of
the first development line is archived in
[archive/2026-10-legacy/PROJECT_FORMAT.md](../archive/2026-10-legacy/PROJECT_FORMAT.md).

A project is one UTF-8 JSON file. Reading it only parses data: no code is
executed, no library is loaded and nothing is downloaded. Files written by a
newer format version are refused with a message instead of being read
partially.

```json
{
  "format": "simnodus-project",
  "version": 1,
  "next_id": 7,
  "settings": {"mode": "interactive", "speed": 1, "max_step": 0.0001, "segment": 1,
               "transient_stop": 0.01, "transient_max_step": 1e-06},
  "components": [{"id": 1, "type": "resistor", "designator": "R1", "at": [10, -4],
                  "rotation": 90, "mirrored": false, "parameters": {"R": "4k7"},
                  "value_offset": [3, 2]}],
  "wires": [{"id": 2, "points": [[13, -4], [20, -4], [20, 3]]}],
  "labels": [{"id": 3, "at": [20, 3], "name": "OUT"}],
  "probes": [{"id": 4, "kind": "current", "target": {"item": 1, "pin": "1", "at": [0, 0]},
              "color": "#1f77b4"}],
  "notes": [{"id": 5, "at": [0, -10], "text": "Close SW1.\nThen watch the LED."}]
}
```

- **Models** (PRJ-07): a component may carry `"models": {"model": ".model MY-NPN NPN(BF=50)"}`,
  the statement of a model that is not built in (chosen from the user's models
  folder), so the project opens the same on another computer
  ([ADR 0095](../decisions/0095-model-variants.md)). For a vendor subcircuit
  the value is the self-contained `.subckt` text, and `"model.pins": "IN-,IN+,V+,V-,OUT"`
  may give the part pin of each port when it differs from the usual order
  ([ADR 0097](../decisions/0097-vendor-subcircuits.md)).
- **Buses and splitters** (UI-19, [ADR 0096](../decisions/0096-buses-and-splitters.md)):
  `"buses": [{"id", "points", "name": "D"}]` are drawings only;
  `"splitters": [{"id", "at", "rotation", "mirrored", "bus": "D", "first": 0, "count": 8}]`
  have their bus point at `at` and bit pins at local (2, 0), (2, 2), ...; each
  bit pin is a net label `D0`, `D1`, ...
- **Blocks** (PRJ-06, [ADR 0098](../decisions/0098-user-blocks.md)):
  `"blocks": [{"name": "RC_lowpass", "source": "blocks/RC_lowpass.snx", "project": {...}}]`
  keeps a copy of each block file the project uses (`project` is the whole
  file, which may have blocks of its own, up to eight levels); `source` is the
  file's path relative to the project's folder. Components use a block with
  `"type": "block:RC_lowpass"`. A project with blocks is written as version 2;
  without them as version 1, which SimNodus 0.3 also reads.
- **Notes** (UI-17) are free text, top-left corner at `at`, lines separated by
  `\n`; they are presentation only and never reach the netlist. Files without
  `notes` load as before.

- **IDs** are positive integers, unique across all element kinds and never
  reused (`next_id` keeps the counter, including gaps left by deleted
  elements). Designators and net names are presentation only.
- **Coordinates** are integer grid points.
- **Parameters** keep the text the user typed (`4k7`); interpretation happens
  when the netlist is generated.
- **Probes** refer to a component pin (`item` + `pin`) or a point on a wire
  (`item` + `at`). If the target is deleted the probe stays and is reported
  as invalid; it is never rebound by name.
- **Settings** out of range are replaced by defaults with a warning.

## Saving

`save_project` writes `<name>.snx.tmp-<n>` in the same folder, copies the
existing file to `<name>.snx.bak` and then renames the temporary file over
the project. A failure at any step leaves the previous project intact.

## Clipboard

Copy/paste uses the same element objects under `"format":
"simnodus-clipboard"`; pasting assigns new IDs, renumbers designators that are
taken and moves probes along with pasted targets. The clipboard carries the
blocks the copied parts use; pasting adds those the project does not have.
