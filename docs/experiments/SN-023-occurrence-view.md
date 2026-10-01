# SN-023: read-only artwork for one existing occurrence

Date: 2026-09-30. Status: implemented and tested for the owned fixture, using
existing native operations. Full SN-023 remains in_progress. No persisted
geometry or placement operation is introduced.

## Acceptance before implementation

Current native project operations do not support saved geometry. Reduce the next
placement increment to a disposable fitted view of **one existing occurrence**.
Use `main/left/r` or `main/right/r` with stable source definition `rc/r` and component
`resistor`. Keep its central selection independent of both library Preview and
Properties, so selecting `main/right` to edit upstream resistance retains the
displayed `main/right/r`. General schematic editing, wiring and instruments remain
separate gates. The local pre-implementation gate is hashed by the
[summary](evidence/SN-023-occurrence-view-summary.json).

- Pure Qt-free application lookup walks root-prefixed IDs through connectivity,
  requires a component at the terminal path and cross-checks the current resolved
  row, definition and source span. Return owned path/source IDs/name/applied values.
  Names, array order and source offsets are not identities across revisions.
  An independent Python declared-ID traversal checks valid reused contexts,
  reordered/escaped keys, duplicate/untrusted labels, absent/rootless/noncomponent
  paths, unreachable definitions and upstream literal changes.
- Open and all selections stay inert. Only **Capture Occurrence Artwork...** reads
  the single known owned SVG through existing native verification from a separately
  requested root. Its immutable application capture owns occurrence metadata and
  an independent symbol artifact, without adopting library Preview/root authority.
- Repaint/tab/catalog/Properties changes leave bytes, history, four drafts and
  resource/runtime flags unchanged. Changing the occurrence path clears its
  capture. The same path/source/component and complete request/map/pins retain
  artwork across name/R/history changes; current labels/values requery the latest
  graph. Failed Open/capture retains the prior marked artifact; successful Open
  clears it and requires explicit selection/capture again.
- One native Windows/local-NTFS lifecycle and one bounded Qt path use only the
  existing right literal resistance edit `2.2 -> 3.5 kohm`, one-step history,
  create-only Save Copy and explicit reopen. Independently audit the saved bytes,
  stable full IDs, original source and separate single-file roots. Check headless
  execution/imports and inspect the final image. Reuse unchanged backend, worker,
  edit and layout matrices; retain intermediate/failed/inconclusive evidence.
- Preserve all 228 earlier evidence files and twelve independent SN-045 overlays;
  update CURRENT/task/handoff, run repository/staged-tree and exact-source required
  Foundation checks, then the authorized branch/PR/squash workflow.

This extends existing inspection/capture/presentation without a general geometry
schema or architecture decision; no new ADR is needed. Preserve the independent
windows, Components/Preview versus Properties, adjustable panels and shared
instrumentation ownership. Domain, application, adapters, instrumentation and Qt
presentation remain separate; no engine or simulation work runs in this slice.

## Implemented behavior

`inspect_component_occurrence` follows the full ID vector from the validated root
through circuit instances. It refuses a circuit/net/absent path, checks the final
component definition against the resolved snapshot and current source object,
and returns owned strings/parameter rows. Inspection can describe other existing
component metadata; artwork capture supports only the previous exact owned
`resistor` SVG and private two-pin convention. Unsupported artwork remains blank.

`capture_fixture_occurrence` composes that owned selection with a new explicit
`capture_fixture_symbol`. It never reuses the library capture implicitly.
`fixture_capture_matches` centralizes the previous full request/map/optional-pin
comparison for both independent views, with no resource I/O. Applied values,
display names and source offsets do not participate in artwork retention.

The central **Structure** / **Occurrence (read only)** tabs retain existing
structural selection and offer a separate full-path component selector. Choices
come directly from already validated resolved rows filtered by component IDs;
only the selected path receives the complete lookup. Each graph refresh restores
that path and current metadata independently of Properties. The tab reuses the
unchanged fitted `PreviewCanvas`; it adds no position, drag, snapping or wire.
The plain caption states the full path, source definition, current name, applied
resistance/binding origin and the lack of saved position/runtime measurement.
Capture root/path/hash remain in the tooltip. No paint rereads a resource.

The demo helper only adds optional instructions for this tab; its preparation and
launch semantics are unchanged. To try the latest local build, use a fresh output:

```text
python tools/run_editor_demo.py --editor build/sn023-occurrence-view/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/occurrence-demo-01
```

Follow printed Open instructions, select `main/right/r` in **Occurrence (read only)**,
then explicitly **Capture Occurrence Artwork...** from the printed resource root.
Select `main/right` in Structure/Properties and apply `3.5` kohm. The central
selection/capture survives while its applied caption changes to `3500 ohm`.
Save Copy uses a new filename; explicitly Open it, reselect the occurrence and
request capture again. The fitted display has no saved geometry.

## Reconfirmed baseline and measured results

PR #79 was reconfirmed MERGED, source `32f91eb3277bc233f4b2578c67acaf22292a3318`,
squash `1790b44dd7f6893978f5e100bf0dad7428131400`, Foundation Windows 66/66 and
Ubuntu 55/55 successful. Local main, origin/main and remote main were equal, with
zero unpublished commits. Branch `codex/sn-023-occurrence-view` starts there. All
228 prior evidence files and twelve overlays were backed up before editing.

- Three focused Release CTests passed first run: new occurrence inspection plus
  the existing native Preview and document contracts. The new suite passed six
  tests, 34 independent selections and one lifecycle (35 requests). Its original
  four component paths, shadow source definition with reused local IDs, unreachable
  definitions, upstream R values, escaped keys/order/untrusted labels and refusals
  matched the independent oracle. Pure output survives raw/graph owner release.
- The native lifecycle passed 14 assertions and one independent saved-byte audit.
  Only existing right R changes; original document and single separately rooted SVG
  remain unchanged, with model/LICENSE absent. Current labels/values, immutable
  captured metadata, history, failed operations, exact copy/reopen and release of
  document/graph owners are checked. The saved ID remains `main/right/r` and `rc/r`.
- The same native binary passed 14 assertions/one byte audit with QT_* removed and
  PATH restricted to System32; direct PE imports contain no Qt. No backend ran.
- [GUI 01](evidence/SN-023-occurrence-view-gui-01.json) passed 25 controls/one byte
  audit and its image was inspected. Review then found repeated complete lookups
  while listing every occurrence. Source/executable/native probe snapshots are
  retained; this is a code-review finding, not a measured large-project GUI failure.
  Listing now filters validated rows and checks only the current selection.
- [GUI 02](evidence/SN-023-occurrence-view-gui-02.json) passed 25 controls/one byte
  audit after that Qt-only refinement. Both builds passed; the native binary and
  native source remain unchanged after their tests, so no native matrix was repeated.
  Final image shows complete caption/button, independent library/occurrence artwork,
  `main/right/r` and applied `3500 ohm`. Repaint was exercised while its copied SVG
  pathname was temporarily absent, then the exact file was restored.
- Qt checks cover four retained Properties drafts, independent roots/captures and
  tab/catalog/Properties selections, left/right values, unsupported capacitor,
  name/R/history, failures, copy/reopen and current metadata. Direct native reloads
  inside the harness check equivalent ordering retention and changed declared map
  clearing; these are not a user binding edit, Open workflow or recovery flow.
- Native temporary-case enumeration reported access denied in the read-only agent
  context. The host context read/audited all six retained files and produced ordinary
  byte-identical readable copies without changing originals or permissions. That
  collection observation is preserved; it proves no product ACL/recovery guarantee.

The [summary](evidence/SN-023-occurrence-view-summary.json) pins sources, binary,
commands, both GUI reports and local captures/lifecycle/collection artifacts.
Source PR records final checker/required-check/squash integration. Only this task's
CURRENT/BACKLOG prefix/row is published; prior failed runtime/setup/visual and
inconclusive desktop-launch evidence stays unchanged. No binary/release is published.

## Pending behavior and next bounded gate

Next establish acceptance for one read-only declared terminal/net correspondence
on the selected existing occurrence, using stable IDs and stating whether it is
local declaration membership. Do not imply flattened electrical/model execution
or add wiring/placement edits as part of that gate. General symbols/anchors,
geometry/schema, components/properties, multi-step undo, instruments/shared capture,
managed Save, production workers/IPC and simulation integration remain pending.

Four test-only layout event passes before capture do not prove user recovery,
large-project responsiveness, keyboard/accessibility, monitor/DPI or cross-session
layout. Human dialogs/normal close and packaging stay pending; the earlier demo
window-discovery result remains inconclusive. Qt licensing/provenance/redistribution
and installed runtime closure are not expanded by a local build.

Preserve SN-021 Windows/local-NTFS/one-managed-document/explicit fixed-E-01 limits,
all unproven external overwrite/executable access/service/MCU/mixed-signal/power-loss/
trusted-origin/redistribution behavior, SN-022 synthetic nine process/nine layout/
Qt-free-worker selection with failed attempts, and SN-017 Python preparation/GDB/
fixture control, accepted profiles/tolerances, MCU/toolchain independence and
PDF suppression/PID retries. January stabilization and February 2027 classroom
use remain planning targets.
