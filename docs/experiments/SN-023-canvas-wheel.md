# SN-023: bounded vertical wheel zoom on the fixed RC canvas

Date: 2026-10-05. Status: implemented and tested on native Windows for the existing
fixed RC view. SN-023 remains in_progress; general schematic editing, wiring and
desktop simulation remain pending.

## Acceptance and implemented behavior

The owner confirmed that middle-button pan worked and authorized continuation.
This is informal positive feedback, not full usability acceptance. The gate and
exact 25-control/16-stage contract preceded implementation and are retained and
hashed in the [summary](evidence/SN-023-canvas-wheel-summary-02.json).

Place the pointer over the canvas and roll an ordinary vertical mouse wheel.
Each complete 120-unit angle detent calls the existing zoomIn/zoomOut operation,
using centered 50/75/100/125/150 percent levels. Integer division discards a partial
event remainder without accumulation. Clamp the number of steps to [-4,4] before
iterating, including extreme integer deltas. Scaling preserves the existing scene
pan; it is not anchored at the pointer. Buttons and percentage refresh after a
changed scale. Existing buttons remain available.

Accept NoScrollPhase, NoModifier, non-inverted events with zero pixelDelta, zero
horizontal angleDelta and either no buttons or only MiddleButton. Supported
complete events are consumed and cancel an active middle drag, including at a
scale boundary. Release and press the middle button again to start another drag.
Zero/partial, horizontal (including diagonal), pixel, phase, inverted, modified,
primary-button and unsupported-diagram events are ignored without changing view,
drafts, selection or an active drag. This bounded path does not accept trackpad or
high-resolution remainder gestures.

Graph/IDs/values, selection, independent R/C activation, four drafts and native
Apply/Undo/Redo/Save Copy stay intact. Existing Fit, successful Open and unsupported
shape resets remain unchanged. Failed Open retains the view and drafts. The footer
explains wheel, middle drag and Fit. No schema/parser/application/domain/engine/
worker/resource/Qt-module change is added. Qt Core/Gui/Widgets remain in presentation;
simulation stays off the GUI thread. Independent Circuit Editor and Signal Analyzer
windows, Components/Preview versus instance Properties, adjustable panels and shared
instrumentation ownership remain intact.

## Measured evidence and reuse

Before editing, PR #90 was reconfirmed MERGED: source
bb27ac61228f168f160a2fc8f7c0d7f2db5892bd, squash
84ab9ba0e58f820673445b2bab7fca4c6676c0a3. Required exact-source Foundation Windows
67/67 and Ubuntu 56/56 passed in workflow 37311502805. Local main, origin/main and
live remote main matched, with zero unpublished commits. Branch
codex/sn-023-canvas-wheel preserves 267 historical evidence files and twelve
independent local SN-045 overlays.

- Fresh native Windows C++20 Release configure and five incremental builds passed
  using Qt 6.11.1, Windows platform and Fusion style. Before the wheel run, actor
  refusal coverage during drag, the LF convention and the expected Zoom label
  were refined. No configure, build or GUI attempt failed in this slice. One
  subagent write tool initially reported model capacity before executing; it wrote
  nothing and the same approved retry succeeded. This is not user recovery evidence.
- [Final wheel GUI02](evidence/SN-023-canvas-wheel-gui-02.json) passed 25 controls,
  sixteen independently source-resolved snapshots, 30 exact component captions,
  twelve Properties/base-value blocks and two exact saved-token audits.
  The [independent audit](evidence/SN-023-canvas-wheel-audit-02.json) compares literal
  expected scale, pan, drag flags, dimensions, rectangles and R/C points.
  Offset/rectangle tolerance is 0.000001; point tolerance is one pixel. Actor clicks
  use literal R/C centers and independently maintained expected pan, never production
  componentPoint or the reported rectangle. Decimal caption and source JSON oracles
  reuse unchanged implementations.
- Coverage includes positive/negative/full/extreme detents, both limits, partial/
  zero and guarded refusals during an active drag, valid-event cancellation at and
  below the upper limit, button/Fit interoperability, retained selection/drafts/pan
  through resize/Details/context and stale Apply refusal, independent C/R edits,
  native history, pending copy/history refusal, create-only/occupied copies, failed
  Open and successful reopen/unsupported resets. R/C hit tests follow wheel scaling.
- The intermediate copy changes only right C from 220 to 470 nF. The combined copy
  adds only right R from 2.2 to 3.5 kohm. Other bytes, original/variant inputs and
  fixture bytes remain unchanged; the roots contain no resources.
- The unchanged [pan driver on the final binary](evidence/SN-023-canvas-wheel-pan-03.json)
  passed 28 controls, 23 snapshots, 44 captions, twenty Properties blocks, two exact
  copies and independently derived original-port visibility. Its
  [audit](evidence/SN-023-canvas-wheel-pan-audit-03.json) retains the result. An earlier
  pan run also passed; its local report, runner, raw logs and captures remain intact.
- Three wheel images were inspected. At 50 percent, the pending R 3.5 field remains
  separate from applied 2.2 kohm. At 150 percent/pan 50,-10, the pending C 470 field
  remains separate from applied 220 nF; output extends beyond the canvas. Fit restores
  the full normal view with legible applied 3.5 kohm/470 nF, selection, target/unit/
  limits/field/Apply and navigation hint. Fifteen captures across wheel and all three pan
  runs are retained. Synthetic events and image inspection do not establish physical
  mouse, OS focus/capture, user recovery, keyboard/accessibility, monitor/DPI,
  arbitrary long labels, cross-session layout or packaging acceptance.
- Twenty-seven sources and the final binary are frozen. Twenty-two prior source/
  binary entries, fourteen native artifacts and five byte-identical caption/resolver/
  selection/native operation/previous C/R/Zoom/Pan acceptance sections match reuse
  hashes. Independent static review found no additional actionable defect.
  Zoom 25, R 43, C 41, caption 29, canvas 33/18, occurrence 80/11/two audits, native
  150 requests, lifecycle 14 assertions/one audit, no-Qt and Preview/backend/worker/
  layout evidence are reused without local matrix reruns. All earlier positive,
  negative and inconclusive evidence remains, including the first pan failure,
  preceding zoom clipping result and SN-022's first setup/runtime failures.

The first publication guard refused a source hash mismatch: one new blank CRLF
was normalized by the index. Its diagnostic and initial source/binary freeze are
retained. Only that byte was corrected; a new build and wheel25/pan28 runs passed
on the final source/binary freeze. Earlier passing runs remain intact. This scripted
retry does not establish a user recovery flow.

The source PR records repository/exact task-only checkers, required exact-source
Foundation checks, guarded squash and final main confirmation. Planning publication
contains only the new task prefix and SN-023 row, excluding all twelve overlays.
No significant architecture decision is selected; no ADR is added.

## Try the local build

Use the installed development kit and a fresh output root:

```text
python tools/run_editor_demo.py --editor build/sn023-canvas-wheel/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/canvas-wheel-demo-01
```

1. File -> Open the printed document/original.json. Start at 100 percent.
2. Place the pointer over the circuit canvas. Roll the wheel up/down to change one
   level per complete detent, limited to 50-150 percent. The percentage and disabled
   boundary button reflect the view. No modifier key is required.
3. Press the wheel button and drag to pan; release to stop. Rolling during a drag
   stops it. Release and press again to pan. Fit returns to centered 100 percent.
4. Click R/C after navigating. Edit Resistance or Edit Capacitance remains explicit;
   typing alone leaves the applied caption and project unchanged.
5. Apply R 3.5 kohm and C 470 nF, Save Copy to a new leaf such as RC-wheel-test.json,
   then explicitly Open it. Values persist; view/selection/activation reset. Resolve
   pending text before history/copy/target change.

START_CANVAS_WHEEL_DEMO.cmd is retained under build/sn023-canvas-wheel. Launching
is an explicit user action. The executable needs the installed Qt kit; packaging
remains pending. Older roots/processes/user files remain intact; choose another
fresh output root if the proposed root already exists.

## Proposed next gate and pending behavior

Gather feedback, then separately gate a reversible Focus Circuit panel action if
useful. This is proposed only and must retain independent windows and adjustable
Components/Preview and Properties panels. Cursor anchoring, accumulated high-resolution
wheel, trackpad/inverted/modified gestures, general component/library/Preview properties,
placement/rotation/wiring/arbitrary shapes, instruments, managed Save, real-engine
workers, production IPC/general execution and human usability remain pending.

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
