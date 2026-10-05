# SN-023: bounded middle-button pan on the fixed RC canvas

Date: 2026-10-05. Status: implemented and tested on native Windows for the existing
fixed RC view. SN-023 remains in_progress; general schematic editing, wiring and
desktop simulation remain pending.

## Acceptance and implemented behavior

The owner confirmed that Fit/Zoom worked and authorized continuation. This is
informal positive feedback, not full usability acceptance. The preimplementation
gate and exact 28-control/23-stage contract are retained and hashed in the
[summary](evidence/SN-023-canvas-pan-summary.json).

Hold the middle mouse button and drag to move the existing view. The scene offset
clamps to +/-140 horizontally and +/-84 vertically on the fixed 700 by 420 drawing.
Each move uses the previous pointer position, so reversing direction at a limit
moves immediately. The same rectangle positions the drawing, inverse click
coordinates and reported R/C points. The cursor is a closed hand during the gesture.
Pan moves the view, not components or connections, and creates no saved positions.

Selection, graph/IDs/values, independent R/C activation and four drafts remain
intact. Left selection is ignored while dragging; right presses and moves without
an active middle press are inert. Canvas mouse tracking delivers moves without
buttons so a missing middle-button state can terminate the gesture. Release,
ungrab, deactivation, hide, resize and graph refresh also cancel the drag.
Zoom cancels the gesture and retains offset. Fit cancels and resets both offset
and percentage. Context, Details, resize, native edits/history and failed Open
retain offset; successful Open and unsupported shapes reset the view.

The footer explains the gesture and Fit. No schema/parser/application/domain/
engine/worker/resource/Qt-module change is added. Qt Core/Gui/Widgets remain in
presentation. Simulation remains off the GUI thread. Independent Circuit Editor
and Signal Analyzer windows, Components/Preview versus instance Properties,
adjustable panels and shared instrumentation ownership remain intact.

## Measured evidence, failures and reuse

Before edits, PR #89 was reconfirmed MERGED: source
4ff75709e6fd4c3aba6ab81347b2d8dbbccd82e6, squash
4add16905cd60ffc268b9a52fe13d7985b1411d9. Required exact-source Foundation Windows
67/67 and Ubuntu 56/56 passed in workflow 37307334983. Local main, origin/main and
live remote main matched, with zero unpublished commits. Branch
codex/sn-023-canvas-pan preserves 261 historical evidence files and twelve
independent local SN-045 overlays.

- Fresh native Windows C++20 Release configure and both builds passed using
  Qt 6.11.1, Windows platform and Fusion style. No setup or build attempt failed.
- [Pan GUI01 failed](evidence/SN-023-canvas-pan-gui-01.json): seven controls failed.
  Pending target/draft layout changes resized the canvas and correctly cancelled
  the first gesture and later input-port gesture. Separately, without mouse
  tracking, a synthetic no-button move did not reach mouseMoveEvent and left
  panning active. The actor now settles layout before each press; production
  enables mouse tracking. The failed source/helper/binary set, original report,
  runner, raw logs and three captures are retained without overwrite.
- The first wrapper saved raw logs, then failed to print the large diagnostic
  through cp1252. Its UnicodeEncodeError is retained. The wrapper now escapes
  unrepresentable output; the driver's failure message points to raw artifacts
  rather than repeating the entire report. One correction patch failed an exact
  marker check before file writes; its diagnostic and inspected state are retained.
  Neither scripted correction is a tested human recovery flow.
- [Final pan GUI02](evidence/SN-023-canvas-pan-gui-02.json) passed 28 controls,
  23 independently source-resolved snapshots, 44 exact component captions,
  twenty Properties/base-value blocks and two exact saved-token audits.
  The [independent audit](evidence/SN-023-canvas-pan-audit-02.json) compares literal
  expected offsets, drag flags, dimensions, rectangles and component points.
  Offset/rectangle tolerance is 0.000001; point tolerance is one pixel. Qt actor
  clicks use literal centers and independently maintained expected offsets,
  not production componentPoint or reported view geometry.
- Coverage includes both bounds and immediate reversal at the upper bound,
  press/move/release, left/right guards, no-button/ungrab/deactivation cancellation,
  Fit/Zoom/resize/Details/context, stale Apply, independent R/C drafts, native
  Apply/Undo/Redo, pending copy/history refusal, create-only/occupied copies,
  failed Open and successful reopen/unsupported resets. Original input/reference
  points are independently confirmed reachable at offset 90/30; output is
  reachable at -90/30, both at 150 percent. These are declared ports, not inferred
  source/ground or simulation measurements.
- The intermediate copy changes only right C from 220 to 470 nF. The combined
  copy adds only right R from 2.2 to 3.5 kohm. Other bytes, original/variant inputs
  and fixture bytes stay unchanged, and the roots contain no resource files.
- The unchanged [zoom driver on the final binary](evidence/SN-023-canvas-pan-zoom-02.json)
  passed 25 controls, sixteen snapshots, 30 captions, twelve Properties blocks
  and two exact copies. Its [audit](evidence/SN-023-canvas-pan-zoom-audit-02.json)
  retains the results. The earlier zoom run also passed on the first binary.
- Final pan-input, pan-output and Fit captures were inspected. The intended ports
  are visible in each enlarged view; the opposite edge and some component artwork/
  caption may crop. Fit restores the normal full view with legible captions,
  selection, target/unit/limits/field/Apply and navigation hint. Twelve captures
  across both attempts and both GUI paths remain retained. Synthetic gestures,
  cancellations and image inspection do not prove physical mouse, OS focus/capture,
  human recovery, keyboard/accessibility, monitor/DPI, arbitrary long labels,
  cross-session layout or packaging.
- Twenty-six sources and the final binary are frozen. Twenty-one prior source/
  binary entries, fourteen native artifacts and five unchanged caption/resolver/
  selection/native edit/history/copy/previous C/R/Zoom acceptance method sections
  match reuse hashes. R 43, C 41, caption 29, canvas 33/18, occurrence 80/11/two
  audits, native 150 requests, lifecycle 14 assertions/one audit, no-Qt and
  Preview/backend/worker/layout evidence are reused without local matrix reruns.
  All earlier positive, negative and inconclusive evidence, including SN-022's
  first setup/runtime failures and the preceding zoom clipping result, is retained.

The source PR records repository/exact task-only checkers, required exact-source
Foundation checks, guarded squash and final main confirmation. Planning publication
contains only the new task prefix and SN-023 row, excluding all twelve overlays.
No significant architecture decision is selected; no ADR is added.

## Try the local build

Use the installed development kit and a fresh output root:

```text
python tools/run_editor_demo.py --editor build/sn023-canvas-pan/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/canvas-pan-demo-01
```

1. File -> Open the printed document/original.json. Start with the normal view.
2. Zoom In twice to 150 percent. Hold the middle mouse button (the wheel button)
   and drag right to reveal input/reference; drag left to reveal output. Release
   the button to stop. At a limit, reverse direction to move back immediately.
3. Click R/C after panning and inspect their applied values. Edit Resistance or
   Edit Capacitance remains explicit; typing alone leaves applied values intact.
4. Apply R 3.5 kohm and C 470 nF, Save Copy to a new leaf such as RC-pan-test.json,
   then explicitly Open the copy. Values persist; pan/zoom reset and edit activation
   must be requested again. Resolve pending text before history/copy/target change.
5. Use Fit to return to centered 100 percent without editing the project.

START_CANVAS_PAN_DEMO.cmd is retained under build/sn023-canvas-pan. Launching is an
explicit user action. The executable requires the installed kit; packaging remains
pending. Older roots/processes/user files stay intact; choose a different fresh
output root if the proposed root already exists.

## Proposed next gate and pending behavior

Gather feedback, then separately gate wheel zoom for this fixed view if useful.
This is proposed only. General component/library/Preview properties, placement,
rotation/wiring/arbitrary shapes, instruments, managed Save, real-engine workers,
production IPC/general execution and human usability remain pending.

SN-017 Python/GDB/fixture control, accepted profiles/tolerances, MCU/toolchain
independence and PDF auto-open suppression/PID retries remain unchanged. SN-021
stays one Windows/local-NTFS managed document and explicit fixed E-01 replay with
inert Open. External overwrite, automatic executable resources, production Save,
general MCU/mixed-signal execution, power-loss durability, trusted origin and
redistribution permission remain unproven. SN-022 accepted only nine synthetic
process cases, nine layout checks and Qt-free headless worker execution; real
engines/production IPC remain pending. SN-044 ownership remains; January
stabilization and February 2027 classroom use are planning targets.
