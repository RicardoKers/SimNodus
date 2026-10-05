# SN-023: transient Focus Circuit on the fixed RC view

Date: 2026-10-05. Status: implemented and tested on native Windows for the existing
fixed RC view and same window/session. SN-023 remains in_progress; general editor,
wiring, desktop simulation and durable workspace layout remain pending.

## Acceptance and implemented behavior

The owner confirmed that wheel zoom worked and authorized continuation. This is
informal positive feedback, not full usability acceptance. The gate and exact
24-control/12-stage contract preceded implementation and are retained and hashed
in the [summary](evidence/SN-023-circuit-focus-summary.json).

**Focus Circuit**, available in View and the toolbar, temporarily hides
Components/Preview and instance Properties. The same checkable action becomes
**Restore Panels**. It captures the immediately preceding main-window dock/toolbar
state and Components/Preview splitter state in memory. Restoration retains prior
panel visibility, widths, splitter sizes and the tested floating Properties state.
Repeated cycles capture the current layout, rather than an original startup layout.
The snapshot belongs to this window/session; it is not serialized to the project
or disk. No generic workspace persistence API is introduced.

The action requires Circuit mode; empty and unsupported canvases may use it.
Declaration Details refuses focus. While focused, Details and individual dock
toggles are disabled, with Restore Panels still available. The Signal Analyzer
remains an independent window that can close/reopen without changing the document
or focus state. Components/Preview continues to describe library declarations;
Properties continues to describe circuit instances.

Selection, graph/IDs/values, R/C activation, four drafts, relative zoom, scene pan
and native operations remain. The unchanged canvas resize policy cancels a middle
drag when panels change available space. Its existing drawing/hit rectangle follows
the new dimensions; no component position changes. Clicking R/C while focused
changes inspection through the existing path, retaining pending drafts. Restore
Panels uses the current document/selection; it does not restore an older document.
Successful explicit Open resets document/view/activation/history as before while
retaining focus; failed Open retains current state. Opening and focus remain inert.

Only presentation window/header/main dispatch and a new driver change. RcCanvas,
domain/application/schema/native save/resources/engine/worker code and selected
Qt modules remain unchanged. Qt Core/Gui/Widgets remain in presentation. No solver
runs on the GUI thread; neither window owns simulation time or capture storage.

## Measured evidence, retained failure and reuse

Before edits, PR #91 was reconfirmed MERGED: source
ceb2fa318becd8f5fa448fe00f8d6115aff92fff, squash
3c348080390f921c26827cce09f5ccd7aaf731fd. Required exact-source Foundation Windows
67/67 and Ubuntu 56/56 passed in workflow 37316338478. Local main, origin/main and
live remote main matched, with zero unpublished commits. Branch
codex/sn-023-circuit-focus preserves 277 historical evidence files and twelve
independent local SN-045 overlays.

- Fresh native Windows C++20 Release configure passed using Qt 6.11.1, Windows
  platform and Fusion style. First build failed C4456 under warnings-as-errors:
  the actor's floating-geometry locals shadowed its splitter locals. Seven exact
  source/helper snapshots and the raw build log remain retained; no editor binary
  was produced by that failed build. Rename those actor locals; build02 passed.
  No production behavior changed for the correction. Scripted correction is not
  a tested human recovery flow. No setup or GUI attempt failed in this slice.
- [Focus GUI01](evidence/SN-023-circuit-focus-gui-01.json) passed 24 controls,
  twelve independently source-resolved snapshots, 22 exact component captions,
  nine Properties/base-value blocks, five layout round trips and one exact saved
  C-token audit. The [independent audit](evidence/SN-023-circuit-focus-audit-01.json)
  checks four drafts, full IDs, edit and independent activation targets, physical
  field visibility, declared source values/nets, expected scale/pan/drag flags,
  rectangle and component points. Geometry tolerance is 0.000001; point tolerance
  is one pixel. Clicks use literal R/C centers and expected scene offset, never
  production componentPoint or the reported rectangle.
- Layout audits compare visible/hidden and floating flags, dock areas, visible
  widths, splitter sizes and the floating Properties rectangle. Tolerance is two
  Qt logical pixels. Initial dock widths 299/499 and splitter 242/423 restored
  exactly. A second configuration restored widths 309/489 and splitter 423/242
  exactly. Already hidden Components remained hidden; both previously hidden
  panels remained hidden. One Properties window at the selected local geometry
  restored floating/visible with its measured 500-unit width. This proves only
  those same-window/session configurations on this host.
- The measured canvas width increased from 452 to 1262 Qt logical units at
  125 percent and scene pan 35,20, returning to 452 on restoration. R/C inspection,
  native C Apply, pending R copy/history refusal, C Undo/Redo during focus, explicit
  create-only/occupied copy behavior, failed Open and reopen/unsupported resets
  passed. The copy changes only right C from 220 to 470 nF; R stays 2.2 kohm and
  all other bytes remain unchanged. Original/variant/fixture bytes remain; no
  resources are prepared or implicitly accessed.
- The unchanged [wheel driver on the final binary](evidence/SN-023-circuit-focus-wheel-01.json)
  passed 25 controls, sixteen snapshots, 30 captions, twelve Properties blocks
  and two exact copies. Its [audit](evidence/SN-023-circuit-focus-wheel-audit-01.json)
  retains independent declarations, geometry, source bytes and guarded events.
- Three focus images were inspected; six captures across both GUI paths remain.
  The normal enlarged view may crop output. The focused 125-percent/panned view
  makes the circuit wider and exposes output but crops the reference caption at
  the lower edge. These are retained visual limitations, not full notation or
  usability acceptance. Fit remains available to recenter at 100 percent. The
  final restored 100-percent view shows the complete fixture and legible applied
  C 470 nF, target/unit/limits/field/Apply, navigation and Focus Circuit action.
- Twenty-eight sources and the final binary are frozen. Twenty-five prior source/
  binary entries, fourteen native artifacts and five byte-identical caption,
  resolver, edit guard/native operation and prior C/R/Zoom/Pan/Wheel acceptance
  sections match reuse hashes. Independent static method/actor review found no
  further actionable defect. Pan 28, zoom 25, R 43, C 41, caption 29, canvas 33/18,
  occurrence 80/11/two audits, native 150 requests, lifecycle 14 assertions/one
  audit, no-Qt and Preview/backend/worker/layout evidence are reused without local
  matrix reruns. Earlier positive, negative and inconclusive evidence remains,
  including the normalized-CRLF publication refusal, first pan failure, preceding
  clipping results and SN-022's first setup/runtime failures.

The source PR records repository/exact task-only checkers, required exact-source
Foundation checks, guarded squash and final main confirmation. Publication contains
only the task's new planning prefixes/row, excluding all twelve overlays. This
implements an already proposed SN-044 view action; no significant architecture
decision is selected and no ADR is added.

## Try the local build

Use the installed development kit and a fresh output root:

```text
python tools/run_editor_demo.py --editor build/sn023-circuit-focus/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/circuit-focus-demo-01
```

1. File -> Open the printed document/original.json. Resize the side panels and
   the Components/Preview divider if desired.
2. Click Focus Circuit in the toolbar or View menu. Both panels disappear and
   the canvas expands. Zoom, wheel, middle drag, Fit and R/C inspection remain.
3. Click Restore Panels. Their preceding sizes and visibility return. A panel
   already closed before focus stays closed; reopen it through View when desired.
4. Select R, activate Edit Resistance, type 3.5 without Apply, then enter/leave
   focus. The field should retain 3.5 while the applied caption still reads 2.2 kohm.
5. Open Signal Analyzer from View. It remains independent of focus and panel
   restoration. Details is available again after restoring panels; leave Details
   to return to Circuit before using focus.

START_CIRCUIT_FOCUS_DEMO.cmd is retained under build/sn023-circuit-focus. Launching
is an explicit user action. Installed Qt is required; packaging remains pending.
Prior roots/processes/user files remain; choose another fresh output root if needed.

## Proposed next gate and pending behavior

Gather feedback, then separately gate bounded keyboard navigation if useful.
This is proposed only. No full keyboard/accessibility, monitor/DPI, physical user
recovery, cross-session restoration or general floating workspace acceptance follows
from synthetic actions and the one local floating case. Unexpected Qt restoration
failure is not a validated recovery flow. Cursor anchoring/high-resolution/trackpad,
general editor/library/placement/wiring/rotation/arbitrary shapes, instruments,
managed Save, real-engine workers, production IPC/general execution and packaging
remain pending.

SN-017 Python/GDB/fixture control, accepted profiles/tolerances, MCU/toolchain
independence and PDF auto-open suppression/PID retries remain unchanged. SN-021
stays one Windows/local-NTFS managed document and explicit fixed E-01 replay with
inert Open. External overwrite, automatic executable resources, production Save,
general MCU/mixed-signal execution, power-loss durability, trusted origin and
redistribution permission remain unproven. SN-022 accepted only nine synthetic
process cases, nine layout checks and Qt-free headless worker execution; real
engines/production IPC remain pending. Scripted retries did not establish a user
recovery flow. SN-044 ownership remains; January stabilization and February 2027
classroom use are planning targets.
