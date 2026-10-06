# SN-023: double-click R/C activation

Date: 2026-10-05. Status: implemented/tested on native Windows for existing
fixed RC fields. SN-023 remains in_progress; general editing remains pending.

## Acceptance and implementation

The owner confirmed the keyboard demo worked and authorized continuation. This
is informal positive feedback, not complete usability acceptance. The gate and
literal 20-control/11-state contract preceded production/test edits; hashes and
retained paths are in the [summary](evidence/SN-023-canvas-double-click-summary.json).

Unmodified left-button **double-click on R/C** inspects through the shared fixed
RC hit/selection path, then invokes the existing guarded begin-edit operation.
Successful activation focuses and selects all text in the same fixed-unit field.
It restores Focus Circuit's immediately preceding panel layout and explicitly
opens Properties if previously closed. Previously hidden Components stay hidden.
Refused activation keeps drafts, edit/activation targets and focus mode. A single
click still inspects; blank hits cannot activate a stale selection. Modified,
other-button, active-middle-drag, empty and unsupported events do not activate.

The gesture changes inspection/activation only. **Apply and Save Copy remain
explicit**. R/C drafts remain independent; existing literal-only and pending-
instance restrictions remain. No new dialog, field copy, generic gesture system,
resource access or engine action is introduced. Qt Core/Gui/Widgets stay in
presentation; domain/application/schema/native operations/instrumentation/adapters
remain separate and unchanged. No simulation work runs on the GUI thread.

## Validation, review and reuse

Pre-edit PR #93 source 2cd00033b9648a31ce01645a487624ca6fb94c32 was reconfirmed
MERGED as c413f2f1e67b19111f28380f56841b9df7ea665e. Exact Foundation Windows
67/67 and Ubuntu 56/56 passed in 37361670954; local/origin/live remote main equal,
zero unpublished. Branch codex/sn-023-canvas-double-click preserves all 291 old
evidence files and twelve independent local SN-045 overlays.

- Fresh native Windows C++20 Release configure/build01 passed with Qt 6.11.1,
  Windows platform and Fusion. First GUI/keyboard paths passed. Independent
  review strengthened the native C Apply predicate to compare with the graph
  immediately before Apply, rather than before a fresh Open. Independent source
  states and exact-token audits already prevented a false overall PASS. Nine exact
  source/helper/binary entries remain. Production behavior did not change for
  this improvement; build02 and both final GUI paths passed. No setup/build/GUI/
  visual attempt failed in this slice; all first-validation evidence remains.
- Final [double-click GUI02](evidence/SN-023-canvas-double-click-gui-02.json) and
  [independent audit](evidence/SN-023-canvas-double-click-audit-02.json) passed
  20 controls, 11 source-resolved states, 20 captions, nine Properties blocks and
  two exact token copies. Positive events use the complete press/release/double/
  release sequence at one unchanged pointer position, even if inspection resizes
  a panel. Coordinates use literal R255,190/C445,280 and expected scene pan, never
  production points. Geometry tolerance is 0.000001; component points one pixel.
- Four drafts, context/full IDs, edit and independent R/C activation targets,
  field focus and selected text are checked against literal expected states and
  declarations. Repeated R/C activation, blank/modifier/button/drag guards,
  zoom/pan hits, pending left-target refusal and missing left C override passed.
  Focus restoration retains previous hidden Components; closed Properties reopens
  explicitly for C. The analyzer remains an independent window.
- Explicit C470 Apply retains pending R3.5 and refuses copy/history until R resolves.
  C Undo/Redo and a create-only copy change only C220->470. Explicit R3.5 Apply
  and the second copy change only R2.2->3.5 relative to the C copy. Occupied-copy/
  failed-Open refusal, reopen resets and unsupported inert activation passed.
  Original/variant/source fixture bytes remain; no resource directories appear.
- Unchanged [keyboard driver on final binary](evidence/SN-023-canvas-double-click-keyboard-02.json)
  and [audit](evidence/SN-023-canvas-double-click-keyboard-audit-02.json) passed
  20 controls/10 states/33 routed events/18 captions/eight Properties/one R copy.
  The earlier [GUI01](evidence/SN-023-canvas-double-click-gui-01.json) and
  [keyboard01](evidence/SN-023-canvas-double-click-keyboard-01.json) remain recorded.
- Three activation images inspected; final images are byte-identical. R at100
  shows a legible full circuit and selected 2.2 field. C/restored 125 at pan 35,20
  can crop output; explicit Fit remains. Ten captures retained across all paths.
- Freeze 30 sources/final binary; verify 25 prior entries, fourteen native artifacts
  and eight unchanged sections: caption/resolver/edit/native operations, all prior
  C/R/Zoom/Pan/Wheel/Focus/Keyboard actors, key/wheel handlers, zoom geometry and
  pan handlers. Reuse other local backend/native/Preview/worker/layout matrices.
  Historical positive/negative/inconclusive evidence remains, including keyboard
  preparation/LNK/UTF-8, Focus C4456, wheel CRLF and first SN-022 setup/runtime.

The source PR records working/exact task-only checkers, required exact-source
Foundation checks, guarded squash and main confirmation. Only the task's planning
prefixes/row are published from shared files. No significant decision or ADR.

## Try the local build

```text
python tools/run_editor_demo.py --editor build/sn023-canvas-double-click/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/canvas-double-click-demo-01
```

1. File -> Open the printed document/original.json. Single-click R to inspect;
   double-click R to activate the existing 2.2 kohm field with text selected.
2. Type 3.5 without Apply, then double-click C and type 470. Switching R/C should
   retain both drafts; the circuit still shows 2.2 kohm/220 nF until explicit Apply.
3. Focus Circuit, then double-click R. Panels return and the pending R field is
   selected. If Properties was previously closed, double-click opens it for editing.
4. Resolve other pending fields before Apply/history/copy. Apply C470, then use
   explicit Save Copy with a new filename. Original source remains unchanged.

START_DOUBLE_CLICK_DEMO.cmd is retained under build/sn023-canvas-double-click.
Launch remains explicit and requires installed development Qt. Preserve prior
roots/processes/user files; choose a fresh output root if already occupied.

## Proposed and pending

Next interaction feedback, then another separately bounded useful fixture path.
Full keyboard/accessibility/physical recovery/monitor-DPI/cross-session layout/
packaging/general editor/library/placement/wiring/instruments/managed Save/real
workers/production IPC remain pending. Keep SN-017 Python/GDB/fixtures/profiles/
tolerances/MCU-toolchain/PDF suppression/PID behavior. SN-021 stays one Windows/
local-NTFS managed document with inert Open/explicit fixed E-01; overwrite/
implicit executable resources/production Save/general MCU-mixed-signal/power-loss/
trust/redistribution remain unproven. SN-022 only selected synthetic nine process/
nine layout/Qt-free headless worker behavior; retries never proved human recovery.
SN-044 independent windows/Components+Preview versus instance Properties/
adjustable panels/shared instrumentation remain. January stabilization and
February 2027 classroom use remain planning targets.
