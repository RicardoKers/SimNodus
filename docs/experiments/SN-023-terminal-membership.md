# SN-023: read-only declared local terminal membership

Date: 2026-10-01. Status: implemented and tested for the owned fixture;
full SN-023 remains in_progress. No connection edit or electrical acceptance.

## Acceptance before implementation

Extend the [existing occurrence view](SN-023-occurrence-view.md) with the logical
pins of one selected existing component and their directly containing source
circuit's declared net membership. The initial local acceptance example confused
symbol anchors with net IDs; before production edits it was corrected to the actual
fixture: `main/right/r` pin `p` belongs to `main/right/drive`, and `n` to
`main/right/junction`. Both criterion versions are retained and hashed by the
[summary](evidence/SN-023-terminal-membership-summary.json).

- Own every pin ID/name, optional local net ID/name and full containing occurrence
  path after graph/raw-owner release. IDs govern lookup, independent of labels,
  array order, symbol maps, available artwork and offsets across revisions.
- Display absent local membership explicitly. Never infer a flattened net,
  physical connection, voltage reference, model terminal or runtime readiness.
  `main/right/drive`, `main/left/drive` and `main/drive` remain distinct paths.
- Selection needs no artwork capture/resource I/O. Keep Properties/library Preview,
  four drafts and existing name/R/history/Save Copy behavior independent. Requery
  current memberships after graph refresh; frozen artwork metadata is provenance.
- Compare native inspection with an independent declared-JSON oracle. Exercise
  original R/C paths, reused/deeper contexts, unconnected/changed membership,
  untrusted/reordered labels/collections, absent paths and swapped/unavailable symbols.
  Check a bounded Qt path, independent saved-token audit, headless execution/imports,
  visual output, repository and task-only publication trees.
- Preserve all 231 earlier evidence files and twelve independent overlays. Publish
  only this task through branch/PR/exact-source required Foundation checks/squash.
  Reuse unchanged backend, worker and layout evidence with all its limitations.

No new architecture choice is made, so no ADR is added. Qt Core/Gui/Widgets remain
presentation-only. Domain/application/instrumentation/adapters remain separate;
independent editor/analyzer windows, adjustable panels and shared instrumentation
ownership are preserved. No engine or simulation work runs in this increment.

## Implemented behavior

`inspect_component_occurrence` extends the existing owned view with every declared
component pin, sorted by ID. It scans only the source circuit reached through the
full occurrence path and matches `InstanceTerminal` by instance/pin IDs. A local
net path consists of the containing occurrence path plus that net's declared ID.
Unknown/duplicate memberships are refused defensively; the existing project loader
already validates these declarations. Ports are not traversed. Missing membership
returns an absent optional net, not an invented net or a physical floating verdict.

The central **Occurrence (read only)** tab adds a read-only table with pin ID/name,
local net ID/name/path and explicit local-declaration scope. It works before artwork
capture and for the capacitor whose artwork is unsupported. Labels are plain cell
text; redundant tooltips are omitted to avoid interpreting untrusted markup.
Selection and painting do not read resources. Existing artwork retention continues
to compare its full symbol binding; changing a local membership does not reinterpret
the captured geometry or grant new resource authority. Current table rows requery
the current graph, independently of frozen capture metadata and Properties.

Successful Open clears the selection/table/captures/history. Failed Open retains
the marked prior document and membership. Existing R/name edits and one-step
history retain stable paths; explicit create-only Save Copy changes only the
previously supported source token. No net/pin/geometry/schema/save operation is added.

## Measured results and retained observations

PR #80 was reconfirmed MERGED: source `802fdbaafeac8c9b96c04700a5bf16ad26504b5e`,
squash `bab9f24e165a8a638576f7cc846fda04598eb7a8`, required Foundation Windows
67/67 and Ubuntu 56/56 successful. Local main, origin/main and remote main matched,
with zero unpublished commits and twelve independent overlays before editing.
Branch `codex/sn-023-terminal-membership` starts there.

- Both Release builds passed. Review after Build01 found the new label tooltips
  could interpret untrusted text as rich text. Initial sources/binaries were retained
  before removing those redundant tooltips. This is a code-review finding, not a
  measured hover-triggered read or failed GUI run. Build02 precedes all runtime tests.
- Three focused CTests passed first run: extended occurrence inspection plus the
  existing native Preview and document contracts. Eight occurrence tests passed
  52 independent selections and one lifecycle (53 requests). The independent
  declared-JSON oracle checks all owned terminal rows after graph/raw-owner release,
  nested `main/nested/inner/r` identities, local net collisions and null/swapped symbols.
- The native Windows/local-NTFS lifecycle passed 14 assertions and one saved-byte
  audit. The same binary passed with QT_* removed and PATH restricted to System32;
  direct PE imports contain no Qt. Original input and separate single-SVG root stay
  unchanged. All retained lifecycle artifacts are readable in the working context.
- [GUI01](evidence/SN-023-terminal-membership-gui-01.json) passed 36 controls and one
  independent saved-byte audit. The image was inspected: both full local paths,
  independent library/occurrence artwork, complete caption/button and applied
  `3500 ohm` are visible. Four drafts, left/right/capacitor contexts, existing edits,
  history, failure retention, exact copy/reopen and absent selection are checked.
  Changed-net reloads show fresh IDs/labels or explicit local absence while retaining
  equivalent captured artwork; swapped symbol maps preserve logical membership.
  Every new table cell is read-only with an empty tooltip.
- Test-only direct native reloads exercise changed declarations; they are not user
  wiring edits, a supported recovery flow or managed saving. Four event-processing
  passes settle the captured layout; no monitor/DPI/keyboard guarantee follows.
  No backend/worker/layout matrix was rerun. The source PR records the final
  repository/published-tree checker and exact-source Foundation/squash results.

## Try the local build

Use the unchanged demo helper with a fresh output directory:

```text
python tools/run_editor_demo.py --editor build/sn023-terminal-membership/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/terminal-demo-01
```

Follow printed Open instructions, then select `main/right/r` in **Occurrence
(read only)**. The table works without capture. `p` shows `main/right/drive`,
`n` shows `main/right/junction`; selecting `main/left/r` changes only the containing
occurrence prefix. Optional artwork capture still requires the explicit printed
resource root. Edit `main/right` to `3.5 kohm` through existing Properties,
Undo/Redo or Save Copy/reopen; local identities remain stable. This is a local
walkthrough using an installed kit, not an installer or visible-human acceptance.

## Pending and next bounded gate

Next separately gate read-only declared peer endpoints within a selected local net,
using explicit endpoint kinds/full IDs and no hierarchy flattening. This may make
the existing fixture more inspectable before any wiring edit is introduced.
Placement/wiring, general components/properties/symbols, multi-step undo,
instruments/shared captures, managed Save and real-engine worker/IPC remain pending.
Human dialogs/normal close, visible desktop interaction, keyboard/accessibility,
large-project responsiveness, monitor/DPI/session layout and packaging remain pending.
The earlier demo visibility observations remain inconclusive.

Preserve SN-017's Python/GDB/fixture ownership, profiles/tolerances, MCU/toolchain
independence and PDF/PID behavior. SN-021 stays within one Windows/local-NTFS
managed document and explicit fixed E-01 replay; opening remains inert. External
overwrite, automatic executable resources, production save service, general MCU/
mixed-signal execution, power-loss durability, trusted origin and redistribution
remain unproven. SN-022 accepted only nine synthetic process cases, nine layout
checks and a Qt-free worker; failed first setup/runtime evidence remains retained,
and scripted retries are not user recovery. January stabilization and February
2027 classroom use remain planning targets.
