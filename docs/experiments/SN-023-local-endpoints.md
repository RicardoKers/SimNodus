# SN-023: declared endpoints of one selected local net

Date: 2026-10-01. Status: implemented and tested for the owned fixture;
SN-023 remains in_progress. No connection edit or electrical acceptance.

## Acceptance before implementation

Selecting a logical pin in the [existing terminal table](SN-023-terminal-membership.md)
should inspect every direct endpoint of its declared local net, including the
selected pin. For `main/right/r` pin `n`, `main/right/junction` contains the local
port `main/right/output` and component pins `main/right/c/p`, `main/right/r/n`.
The [summary](evidence/SN-023-local-endpoints-summary.json) hashes the local gate
recorded before production edits, baseline preservation, commands and raw artifacts.

- Pure Qt-free inspection owns net identity/name/path and explicit endpoint kind,
  instance/terminal IDs, terminal name, definition and full path after owner release.
  Distinguish local ports, component pins and subcircuit ports; obtain names from
  their directly declared interfaces. Never traverse ports, infer model terminals,
  flatten a net or use labels/symbol anchors as identity.
- Explicit pin selection works before artwork capture. Keep Properties, library
  Preview, four drafts and the independent occurrence capture unchanged. Retain
  full occurrence/pin IDs across refresh, existing R/name/history/copy; clear on
  missing pin/path or successful Open. Report local absence for an unconnected pin.
- Independent JSON oracles compare original, reordered/escaped/untrusted labels,
  reused/deeper contexts, all three kinds, changed/absent membership and null/swapped
  symbols. Run the existing occurrence probe/CTest and document contracts, a
  bounded Qt path, exact saved-token audit and no-Qt execution/import check.
- Preserve all 233 historical evidence files/twelve overlays, first results and
  failures. Check repository/task-only trees and exact-source required Foundation
  checks before the authorized branch/PR/squash workflow. Reuse unchanged Preview,
  backend, worker and layout evidence. Correct the adjacent duplicated README paragraph.

This extends source inspection without selecting a general connectivity API,
schema or transport. No significant architectural choice is made; no ADR is added.
Qt Core/Gui/Widgets remain in presentation. Independent editor/analyzer windows,
adjustable side panels and shared instrumentation ownership remain preserved.
No engine or simulation work runs in this increment.

## Implemented behavior

`inspect_occurrence_local_net` first resolves the full component occurrence and
selected logical pin. Missing/unconnected selections return no details. It scans
only the directly containing circuit's selected declared net. Local-port endpoints
own an empty instance ID and the source circuit definition; instance endpoints own
their component/circuit definition and the declared pin/port name. Paths include
the containing occurrence and endpoint IDs; explicit kind remains part of their
interpretation. Rows sort by kind, instance ID and terminal ID. Metadata is owned;
the graph and raw source may be released before consuming it. Existing loader
validation remains authoritative and unexpected graph/interface references refuse.

The central tab adds a plain read-only member table after explicit pin selection.
Every member is shown, including the selected pin; the caption names the selected
full pin path/local net and states that ports are not traversed. No rich-text
tooltips or resource reads are added. Selection uses pin IDs in UserRole and the
full component path. Signal blocking prevents an implicit selection during table
reconstruction. Current metadata reconsults the graph; frozen artwork remains
independent provenance. An unconnected pin retains its selected ID with empty
members and explicit local absence. Removing the pin/path clears the selection
without rebinding by label or row position. Successful Open requires explicit
reselection; failed operations retain the prior marked document and inspection.

Existing native name/R/history/create-only Save Copy operations remain unchanged.
No graph/schema/geometry/resource/runtime/save operation is introduced. Test-only
direct reloads below are not a user wiring edit or an accepted recovery workflow.

## Measured results and retained observations

PR #81 was reconfirmed MERGED: source `a145f20a539601058efc45abb2ff2ba3442c271d`,
squash `9e8f51b4c903e9d7b2731afc8093a3dcfceb4392`, required Foundation Windows
67/67 and Ubuntu 56/56 successful. Local/origin/remote main matched, zero unpublished
commits and twelve independent overlays remained before editing. Branch
`codex/sn-023-local-endpoints` starts there.

- Both Release builds passed. Two focused CTests passed first run: nine occurrence
  tests and existing document contracts. Native inspection matched 52 occurrence
  selections plus 97 local-net requests; one lifecycle gives 150 total requests.
  JSON oracles check direct root component membership beside two subcircuit ports,
  local port labels versus IDs, depth/reuse, unconnected/changed membership,
  malformed/refused inputs, symbol independence and exact copy preservation.
  Owned native output survives graph/raw release.
- The Windows/local-NTFS lifecycle passed 14 assertions and one saved-byte audit.
  Headless execution repeated that bounded lifecycle and matched one local-net
  result against the independent oracle with QT_* removed and System32 PATH;
  direct PE imports contain no Qt. Original document and single-SVG root remain
  unchanged. No engine was run.
- [GUI01](evidence/SN-023-local-endpoints-gui-01.json) passed 50 controls and one
  saved-token audit. Its image was inspected. Review identified an unexercised
  selected-pin removal criterion; initial sources/three binaries were retained
  before adding that control. This was an evidence gap, not a product failure.
- [GUI02](evidence/SN-023-local-endpoints-gui-02.json) passed 51 controls and one
  saved-token audit after the Qt-only control addition. The selected `n` pin is
  removed from a harness-only declaration with obsolete symbol/model bindings
  disabled; native validation accepts the one-pin declaration and the selection
  clears. Original inputs/resource roots stay byte-identical. Native sources and
  probe remain unchanged after their tests, so their matrix was reused.
- Final image inspection shows all three endpoint paths/types, complete scope
  caption/button, independent library/occurrence artwork and applied `3500 ohm`.
  Controls cover explicit selection, four drafts, left/right/capacitor contexts,
  R/name/history, exact copy/reopen, failures, changed membership, unconnected pin,
  removed pin and missing path. New cells stay plain/read-only without tooltips.
  Review found no actionable production defect.
- Two inline helper/audit preparations failed on PowerShell quoting before their
  intended operation; observations are preserved, and saved scripts complete
  preparation/auditing. No configure/build/runtime failed. Test-only reloads/four
  layout-event passes do not prove human wiring, recovery, monitor/DPI or usability.
  The source PR records final repository/task-only checker, exact-source required
  Foundation and squash/main-preservation results.

## Try the local build

Use the unchanged demo helper and a fresh output directory:

```text
python tools/run_editor_demo.py --editor build/sn023-local-endpoints/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/endpoints-demo-01
```

Follow printed Open instructions, select `main/right/r` in **Occurrence (read only)**,
then select pin `n` in the terminal table. All direct `junction` endpoints appear
without artwork capture. Selecting `p` switches to the local `drive` members.
Optional artwork capture still needs an explicit resource root. Existing right
R edit/Undo/Redo keeps the selected pin and member IDs; Save Copy/reopen requires
reselection. This is a local installed-kit walkthrough, not packaging or human
visible-desktop acceptance.

## Pending and next bounded gate

Next separately gate navigation to one existing peer component by its explicit
endpoint kind/full path, preserving pending drafts and requiring fresh explicit
artwork capture. Do not infer a component from a port or silently cross hierarchy.
Wiring/placement/general components/properties/symbols/history, instruments/shared
captures, managed Save, real-engine worker/IPC and broader execution remain pending.
Human dialogs/normal close/visible interaction/keyboard/accessibility/large-project
responsiveness/monitor-DPI/session layout and packaging remain pending; earlier
demo visibility observations stay inconclusive.

Keep SN-017's Python/GDB/fixture ownership, profiles/tolerances, MCU/toolchain
independence and PDF/PID behavior. SN-021 remains one Windows/local-NTFS managed
document with explicit fixed E-01 replay; opening stays inert. External overwrite,
automatic executable resources, production save service, general MCU/mixed-signal
execution, power-loss durability, trusted origin and redistribution remain unproven.
SN-022 accepted only synthetic nine process/nine layout cases and a Qt-free worker;
first failed setup/runtime evidence stays retained and retries are not user recovery.
January stabilization and February 2027 classroom use remain planning targets.
