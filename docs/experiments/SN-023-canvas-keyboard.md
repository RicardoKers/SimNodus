# SN-023: focused canvas keyboard navigation

Date: 2026-10-05. Status: implemented/tested on native Windows for the existing
fixed RC view. SN-023 remains in_progress; full editor and keyboard/accessibility
usability remain pending.

## Acceptance and implemented behavior

The owner authorized continuation after the transient Focus Circuit increment.
The gate and literal 20-control/10-state contract preceded production/test edits;
their hashes and retained local paths are in the [summary](evidence/SN-023-canvas-keyboard-summary.json).

Click the circuit, or use native Tab navigation until its dashed focus border
appears. With the supported visible canvas focused, unmodified **Page Up/Down**
change one existing 25-percent zoom step per event, including repeat, bounded
50..150. **Home** calls the existing 100-percent Fit and resets scene pan. Zoom
retains pan. Valid keys cancel active middle drag, including at a zoom bound;
losing keyboard focus also cancels drag without moving the scene. Modified,
unknown, empty and unsupported keys delegate to QWidget. Tab/ShiftTab use native
focus routing. The border uses the palette highlight and disappears on focus loss.

Keys belong to the widget with focus. R/C, project and instance fields retain
native Home/cursor/text/Page behavior. There are no global navigation shortcuts,
keyboard pan or component selection/editing controls in this slice. Selection,
stable IDs, applied values, four drafts, R/C activation and native operations stay
intact. Navigation works during Focus Circuit; Restore Panels retains drafts and
activation. The independent Signal Analyzer receives its own keys.

Only RcCanvas/window presentation/header/CLI acceptance dispatch and one new
Python driver change. Qt Core/Gui/Widgets remain in presentation, with no new
module or headless dependency. Domain/application/schema/native persistence/
resources/engines/workers remain unchanged; no simulation work runs on the GUI
thread. This is not a significant architecture selection, so no ADR is added.

## Measured evidence and retained attempts

Pre-edit PR92 source aa39274d20a88d6031839faacb490f9a35cfbb9b was reconfirmed
MERGED as d6b383e3556b7a1c4165c69930c61fb4233be4d0. Exact Foundation Windows
67/67 and Ubuntu56/56 passed in37345146908; local/origin/live remote main matched,
zero unpublished. Branch codex/sn-023-canvas-keyboard preserves282 historical
evidence files and twelve independent local SN-045 overlays.

- Fresh Windows C++20 Release configure passed with Qt6.11.1/windows/Fusion.
  An inline helper failed quote parsing, and the saved installation helper then
  failed filesystem writing before any helper edit. Build01 incorrectly started
  before confirming actor installation, failed LNK2019/LNK1120 with no editor
  binary. Nine exact sources/helpers and raw build01 remain. Correct installation
  using the authorized filesystem escalation; build02 passed.
- [Keyboard GUI01](evidence/SN-023-canvas-keyboard-gui-01.json) and the unchanged
  [Focus01](evidence/SN-023-canvas-keyboard-focus-01.json) passed functionally.
  Two inspected PNGs revealed a footer encoding defect from locale-default helper
  reads on Windows. Nine exact source/helper/binary snapshots remain, along with
  both reports/audits/images. Restore original UTF-8 bytes and make helper reads
  explicit UTF-8; build03 passed. Scripted correction is not human recovery proof.
- Final [Keyboard GUI02](evidence/SN-023-canvas-keyboard-gui-02.json) and its
  [independent audit](evidence/SN-023-canvas-keyboard-audit-02.json) passed20
  controls,10 source-resolved view states,33 focus-routed events,18 exact component
  captions, eight Properties/base-value blocks and one exact R-token copy audit.
  The actor sends key events through QApplication's actual focusWidget rather
  than directly to RcCanvas. Receiver/focus before-after, key/modifier/repeat,
  acceptance, zoom and field cursor positions are independently checked.
- Bounds, repeats, Home, Ctrl/unknown guards, native Tab round trip, R Home/text
  insertion/Page isolation, C/project/instance Home in Details, drag cancellation/
  ignored-key retention, panned hit selection and focused panel restoration pass.
  Source-derived declarations/nets/full IDs, four drafts and independent activation
  targets are checked. Mouse coordinates use literal R255,190/C445,280 and expected
  scene offset; geometry tolerance0.000001 and component points one pixel.
- R Apply/Undo/Redo retain view; an explicit create-only copy changes only right
  R2.2->3.5 kohm, keeping C220 and all other bytes. Occupied copy/failed Open
  refusal, reopen resets, unsupported inert keys and analyzer isolation pass.
  Inputs/variants/source fixture remain unchanged; no resource directories appear.
- Unchanged [Focus02 on the final binary](evidence/SN-023-canvas-keyboard-focus-02.json)
  and its [audit](evidence/SN-023-canvas-keyboard-focus-audit-02.json) passed24/12,
  22 captions/nine Properties/five same-session panel round trips/one C470 copy.
  Two final keyboard images were inspected: full Fit view/applied R3.5, visible
  dashed canvas border, native R-field focus without canvas border and readable
  corrected footer. Four keyboard images inspected; ten total GUI captures remain.
  Existing enlarged/panned clipping limits remain as recorded previously.
- Freeze29 sources/final binary. Verify24 prior source/binary entries, fourteen
  native artifacts and five byte-identical caption/resolver/edit/native-operation/
  complete prior C/R/Zoom/Pan/Wheel/Focus actor sections. Reuse other backend/native/
  Preview/worker/layout matrices. All previous positive, negative and inconclusive
  evidence remains, including SN-022 setup/runtime, Focus C4456, pan failure and
  wheel CRLF publication refusal. Independent static review found no further issue.

The source PR records working/exact task-only checkers, required exact-source
Foundation checks, guarded squash and final main confirmation. Only this task's
planning prefixes and row are published from shared overlay files.

## Try the local build

```text
python tools/run_editor_demo.py --editor build/sn023-canvas-keyboard/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/canvas-keyboard-demo-01
```

1. File -> Open the printed document/original.json. Click the circuit; its dashed
   border indicates keyboard focus. Page Up/Down should show125/100 and stop at
   150/50. Middle-drag, then Home: centered100. Source values stay the same.
2. Select R -> Edit Resistance -> type3.5 without Apply. Home now moves the text
   cursor to the start, with no zoom change. Click the circuit again; Page Up/Down
   navigate while3.5 remains pending and the applied caption stays2.2 kohm.
3. Tab leaves the canvas; ShiftTab returns in the tested native round trip.
   Focus Circuit hides panels; navigation still works. Restore Panels returns
   their preceding same-session layout and draft. The analyzer stays independent.

START_CANVAS_KEYBOARD_DEMO.cmd is retained under build/sn023-canvas-keyboard.
Launch is an explicit user action; installed development Qt is required. Keep
prior roots/user files/processes; use a new output root if already occupied.

## Proposed next step and pending behavior

Gather keyboard feedback, then separately gate one useful existing-fixture
interaction. Full keyboard coverage/tab-order usability/accessibility, physical
recovery, monitor/DPI, cross-session workspace and packaging remain pending.
Cursor anchoring/trackpad/high-resolution wheel, general library/editor/placement/
wiring/rotation/shapes/instruments/managed Save and real workers/IPC remain pending.

Retain SN-017 Python preparation/GDB/fixtures/profiles/tolerances/MCU/toolchain/
PDF suppression/PID behavior. SN-021 remains one Windows/local-NTFS managed
document, inert Open and explicit fixed E-01 replay; overwrite/implicit executable
resources/production Save/general MCU/mixed-signal/power-loss/trusted origin/
redistribution remain unproven. SN-022 remains synthetic nine process/nine layout/
Qt-free headless worker evidence, with failed setup/runtime retained; retries did
not validate human recovery. SN-044 independent windows/Components-Preview versus
instance Properties/adjustable panels/shared instrumentation remain. January
stabilization and February2027 classroom use remain planning targets.
