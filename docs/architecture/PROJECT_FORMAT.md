# Project file format (.snx)

Revision 2 — 2026-10-14 (SN-110, SN-115; PRJ-01..PRJ-03). Implementation:
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
              "color": "#1f77b4"}]
}
```

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
taken and moves probes along with pasted targets.
