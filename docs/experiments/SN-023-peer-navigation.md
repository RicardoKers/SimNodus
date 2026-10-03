# SN-023: explicit inspection of one declared peer component

Date: 2026-10-03. Status: implemented and tested for the owned fixture;
SN-023 remains in_progress. No connection edit or electrical acceptance.

## Acceptance before implementation

Extend the [direct local endpoint table](SN-023-local-endpoints.md) with one
explicit action: select a row and press **Inspect Selected Component**. For
`main/right/r` pin `n`, `main/right/c/p` selects component occurrence
`main/right/c`; the reused left path must select `main/left/c`. Selecting a row
alone is inert. The [summary](evidence/SN-023-peer-navigation-summary.json) hashes
the local gate recorded before production edits, preservation and raw artifacts.

- Reuse the existing owned native inspections and full-path occurrence selector.
  Read explicit endpoint kind/path from UserRole; never use labels or row position
  as identity. Only another component is a destination. Self, local ports,
  subcircuit ports, empty, unconnected and stale selections refuse.
- Activation rechecks the current graph's source occurrence, local net path,
  endpoint kind/instance/terminal/definition/path and target component/pin. Copy
  identity before changing the selector and destroying old rows. No port traversal,
  schema, geometry, resource, save, engine, worker or generic connectivity API.
- Destination component selection clears the old occurrence capture, pin/details
  and peer row/action. Keep Properties/library/four drafts, document bytes,
  dirty/history/save association and independent analyzer unchanged. Pin selection
  and any new artwork capture remain separate explicit actions.
- Retain the 51 existing Qt controls and add bounded peer controls with independent
  declared-JSON fixture/copy/root audits. Include left/right identity, reverse
  navigation, ports/self, reordered rows/equal untrusted labels and an endpoint
  removed before table refresh. Check the final action's layout and image.
- Build only the editor locally in a new directory, preserving prior binaries.
  Reuse unchanged native/Preview/backend/worker/layout evidence by hashes. Gate
  publication through repository/task-only checker and exact-source Foundation
  checks; preserve all 236 historical evidence files/twelve independent overlays.

This changes presentation navigation over existing inspections, without a new
architectural contract. No ADR is added. Qt Core/Gui/Widgets remain in presentation;
no engine or simulation work runs in this increment.

## Implemented behavior

The endpoint table has plain read-only cells, single row selection and one button.
Only a current component-pin member of a different existing component enables the
action. Its UserRole contains the full endpoint ID vector and explicit kind.
Activation matches that owned member against a fresh local-net inspection and
cross-checks the target using the existing component lookup and selector identity.
Source/target definitions and logical pin identity must still agree. Refusal
disables the action and reports unavailability while retaining the prior view,
capture, drafts and current document. No display name establishes identity.

The action selects only the component occurrence. It clears the previous selected
pin/local-net details and occurrence capture through the existing occurrence
callback. It never selects a destination pin, changes Properties/library selection,
discards text, loads resources or starts execution. The capacitor can be inspected
without available artwork. Returning to the resistor requires an explicit fresh
artwork capture. Rebuilding members clears their selection/action with signals
blocked; current applied values remain separate from frozen artwork provenance.

Existing native name/R/C/history/create-only Save Copy operations are unchanged.
Independent editor/analyzer windows, adjustable panels, Components/Preview versus
instance Properties and shared instrumentation ownership remain preserved.

## Measured result and reused evidence

Before editing, PR #82 was reconfirmed MERGED: source
`c2fe79d0f0a4ec1daf959e18d9a1216e9ea6af49`, squash
`4c082d1ffecf25f9fa4da515f231a7d26daa09b4`, exact-source Foundation Windows
67/67 and Ubuntu 56/56 successful. Local/origin/remote main matched with zero
unpublished commits and twelve independent overlays. Branch
`codex/sn-023-peer-navigation` starts there.

- Fresh C++20 Release configuration and editor build passed first run with MSVC
  19.51.36246.0, Windows SDK 10.0.26100.0 and the installed Qt 6.11.1 MSVC x64 kit.
  Only the editor target and its unchanged native dependencies were built locally.
- [GUI01](evidence/SN-023-peer-navigation-gui-01.json) passed all 67 controls on the
  first runtime: 51 retained plus 16 new peer controls. The independent Python
  audit proves one exact right resistance token change from 2.2 to 3.5 kohm in
  Save Copy, unchanged original/variant inputs and two separate one-SVG roots.
- Explicit row selection is inert. Right/left peer navigation preserves their
  containing IDs, four active drafts, applied document and independent library/
  Properties/analyzer. Returning requires fresh explicit capture. Local ports,
  subcircuit ports and self refuse even under direct method calls. Empty/cleared/
  unconnected/missing selections disable/refuse without rebinding.
- A root fixture variant adds one literal resistor beside local `main/source`
  and subcircuit `main/left/input`, `main/right/input` ports. None is a peer
  destination from that resistor. Another variant reverses declaration/row order
  and uses equal HTML-looking Unicode labels; IDs still select the right capacitor.
- A harness-only reload removes `c/p` from `rc/junction` while leaving the capacitor
  present. Activating its old row refuses against current membership before table
  refresh; source capture, four drafts and loaded bytes/history remain retained.
  Refresh removes the obsolete row without rebinding. Existing edit/Undo/Redo
  before and after navigation preserves revision/history and save association.
- Review found no actionable defect. The image was inspected: complete endpoint
  paths, scope caption and enabled peer action, independent artwork and applied
  `3500 ohm` are visible. The native Windows/Fusion palette reflects this host.
  This is a scripted capture, not human usability or physical monitor/DPI acceptance.
- Unchanged native source/probe/oracle, document/symbol operations, canvas, default
  CMake/main/helper/domain files and prior binaries retain their recorded hashes.
  Prior two CTests, 150 native requests, 14 lifecycle assertions/one byte audit and
  no-Qt execution/import proof are reused from the endpoint increment. No local
  native, backend, worker or layout matrix is rerun or relabeled as a new run.
  The source PR records final checker/exact-source Foundation/squash results.
- No setup/build/runtime failure occurred in this slice. All earlier failed and
  inconclusive evidence remains unchanged. Test-only reloads and layout settling
  do not validate a user connection edit, recovery flow or monitor behavior.

## Try the local build

From the repository root, use the existing helper with a fresh output directory:

```text
python tools/run_editor_demo.py --editor build/sn023-peer-navigation/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/peer-demo-01
```

Open the path printed by the helper, select `main/right/r` in **Occurrence (read
only)** and pin `n`. Select endpoint `main/right/c/p`, then **Inspect Selected
Component**. The selector changes to `main/right/c`; choose pin `p` to inspect its
local net and explicitly navigate back via `main/right/r/n`. Artwork remains a
separate explicit capture with the printed resource root. Properties continue to
describe their independent selection. Existing right R/C edit/history/Save Copy
is available there. This installed-kit walkthrough is not packaging or accepted
human visible-desktop use; previous visibility observations remain inconclusive.

## Pending and next bounded gate

Next separately gate the current applied capacitance and binding origin for the
existing selected capacitor, together with the already supported literal C edit/
history/copy path. Keep physical/model truth, flattened connectivity, geometry/
wiring/placement/general components/properties/multi-step history and instruments
pending. Managed Save, real-engine worker/IPC, general execution, human dialogs/
normal close/visible interaction/keyboard/accessibility/large projects/monitor-DPI/
session layout and packaging remain pending.

Preserve SN-017 Python preparation/GDB/fixture ownership, accepted profiles and
numerical tolerances, MCU/toolchain independence and PDF/PID behavior. SN-021 stays
one Windows/local-NTFS managed document and explicit fixed E-01 replay; Open stays
inert. External overwrite, automatic executable resources, production save
service, general MCU/mixed-signal execution, power-loss durability, trusted origin
and redistribution permission remain unproven. SN-022 stays synthetic nine
process/nine layout cases and Qt-free headless worker execution, with failed first
setup/runtime attempts retained; scripted retries are not a user recovery flow.
January stabilization and February 2027 classroom use remain planning targets.
