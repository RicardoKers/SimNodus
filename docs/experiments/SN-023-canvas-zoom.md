# SN-023: bounded Fit and Zoom on the fixed RC canvas

Date: 2026-10-05. Status: implemented and tested on Windows for the existing
fixed RC view. SN-023 remains in_progress; general schematic editing and desktop
simulation remain pending.

## Acceptance and implemented behavior

The owner authorized continuation after compact R editing. This is informal
positive feedback, not complete human usability acceptance. The preimplementation
gate is retained and hashed in the [summary](evidence/SN-023-canvas-zoom-summary.json).

**Zoom Out**, **Fit** and **Zoom In** operate at 50, 75, 100, 125 and 150 percent
relative to automatic Fit. The unchanged 700 by 420 drawing stays centered. One
view rectangle drives painting, inverse click coordinates and reported component
points. Fit restores 100 percent. Enlargement can crop edge ports and labels;
Fit restores the full normal-fixture view. There is no pan in this increment.
The controls disable without a supported diagram and at their scale limits.

Zoom changes presentation only. It preserves IDs, applied values, selection,
independent R/C activation and drafts, and existing native edit/history/copy.
Changing context or Details, resizing, Apply, Undo/Redo and failed Open retain the
percentage; resizing recomputes the base Fit. Successful Open resets Fit,
selection, activations, drafts and history. Unsupported shapes reset Fit and
disable the controls. Zoom is not persisted in the project or across sessions.

R/C target notes now request minimum vertical space for their wrapped text.
This fixes a clipping defect found in the first visual review. The existing
fields, limits, Apply operations and same-widget transfers remain unchanged.
No schema, parser, application, domain, engine, worker, resource or Qt module is
changed. Qt Core/Gui/Widgets remain in presentation. Independent Circuit Editor
and Signal Analyzer windows, Components/Preview versus instance Properties,
adjustable panels and shared instrumentation ownership remain intact.

## Measured evidence and reuse

Before edits, PR #88 was reconfirmed MERGED: source
f956859448a76686da9afc59fb7c785e42a284cf, squash
d6106341f85a7e957a393821864d306593c032bd. Exact-source Foundation Windows 67/67
and Ubuntu 56/56 passed in workflow 37300508197. Local main, origin/main and live
remote main matched, with zero unpublished commits. Branch
codex/sn-023-canvas-zoom preserves 255 historical evidence files and twelve
independent local SN-045 overlays.

- Fresh Windows C++20 Release configure/build passed with Qt 6.11.1, native
  Windows platform and Fusion style. Both builds and all four GUI runs passed
  their automated checks; no setup, build or runtime attempt failed.
- [Initial GUI01](evidence/SN-023-canvas-zoom-gui-01.json) passed the numeric gate,
  but visual inspection found the capacitance target/unit note partially clipped
  in its 150 percent capture. This negative visual result, the capture, initial
  logs and R regression remain retained. Automated PASS alone did not establish
  acceptable layout. The two notes' size policies were corrected, and focused
  zoom checks now also compare allocated note height to wrapped text height.
- [Final GUI02](evidence/SN-023-canvas-zoom-gui-02.json) passed 25 controls,
  sixteen independently source-resolved snapshots, 30 exact component captions,
  twelve Properties/base-value blocks and two exact saved-token audits. The
  [independent audit](evidence/SN-023-canvas-zoom-audit-02.json) derives view geometry
  from widget dimensions and verifies rectangles within 0.000001 and component
  coordinates within one pixel. Synthetic clicks use separately specified
  literal centers, not the production componentPoint helper.
- The intermediate copy changes only right C from 220 to 470 nF. The combined
  copy adds only right R from 2.2 to 3.5 kohm. All other bytes, original/variant
  inputs and fixture bytes stay unchanged, with no resources in either test root.
  Left remains independent. Coverage includes both limits, resize, independent
  R/C hits at 50/150, four drafts, Details transfers, stale context/Apply refusal,
  native edits/history, pending/occupied copy refusals, failed Open, successful
  copy reopen, unsupported shape and clean left R activation at 125 percent.
- The unchanged [R driver on the final binary](evidence/SN-023-canvas-zoom-resistance-02.json)
  passed 43 controls, fourteen independent snapshots, one R-token saved-byte
  audit, 26 captions and twelve Properties blocks. Its
  [audit](evidence/SN-023-canvas-zoom-resistance-audit-02.json) retains independent
  results and process status. This regression is appropriate because the click
  transform and target-note layout changed.
- Final 150 percent and Fit images and the final R regression image were
  inspected. Component captions, selection, scale controls and target/unit/
  limits/field/Apply text are legible in those normal-fixture images. Cropped
  diagram edges at 150 percent are expected; Fit restores all normal ports.
  Twelve captures from both attempts and both GUI paths are retained. Scripted
  retries do not validate human recovery, physical mouse, keyboard/accessibility,
  monitor/DPI, arbitrary long labels, cross-session layout or packaging.
- Twenty-five sources and the final binary are frozen. Twenty prior source/
  binary entries, fourteen native artifacts and unchanged caption/resolver/
  selection, edit guards, native edit/history/copy and previous C/R acceptance
  method sections match reuse hashes. Prior C 41, caption 29, canvas 33/18,
  occurrence 80/11/two audits, native 150 requests, lifecycle 14 assertions/one
  audit, no-Qt and Preview/backend/worker/layout evidence are reused without
  repeating local matrices. All earlier positive, negative and inconclusive
  evidence, including SN-022's first setup/runtime failures, stays byte-preserved.

The source PR records repository and exact task-only checkers, required
exact-source Foundation checks, guarded squash and final main confirmation.
Shared planning publication contains only this task's prefix and SN-023 row;
the twelve independent overlays are excluded. No significant architecture
decision is selected and no ADR is added.

The first documentation-helper run was refused by filesystem permissions on
CURRENT.md before changing planning files. Its diagnostic is retained. An ignored
helper patch reported a marker mismatch after applying changes; the actual files
were inspected before resuming. Resumption verified frozen sources/binary and
used authorized workspace write access. Neither event reran engine matrices;
scripted retries do not validate a human recovery flow.

## Try the local build

Use the installed development kit and a fresh output root:

```text
python tools/run_editor_demo.py --editor build/sn023-canvas-zoom/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/canvas-zoom-demo-01
```

1. File -> Open the printed document/original.json. The normal RC view starts
   at 100 percent, without selection or edit activation.
2. Use Zoom In twice: 125 then 150 percent; further enlargement disables.
   Click R/C to inspect the corresponding applied value. Fit restores the view.
3. Use Zoom Out twice from Fit: 75 then 50 percent; further reduction disables.
   Click R/C and resize the window to check the same declared selection.
4. At 125 percent, Edit Resistance and apply 3.5 in the fixed kohm field;
   Edit Capacitance and apply 470 nF. Zoom remains 125 percent. Unapplied text
   retains its independent draft and leaves the diagram's applied value intact.
5. Save Copy to a new leaf such as RC-zoom-test.json. Explicitly Open the copy:
   values persist, while Fit returns to 100 percent and edits must be activated
   again. Resolve pending field text before history, copy or changing edit target.

The optional START_CANVAS_ZOOM_DEMO.cmd is retained under build/sn023-canvas-zoom.
The executable needs the installed kit; packaging remains pending. Older demo
roots, processes and user files remain intact. Choose a different fresh output
root if this root already exists. Launching the helper is an explicit user action.

## Proposed next gate and pending behavior

Gather feedback and separately gate bounded pan for this fixed view, if useful
for reaching enlarged edges. This is proposed only. General component/library/
Preview properties, placement, rotation, wiring, arbitrary shapes, instruments,
managed Save, real-engine workers, production IPC and general execution remain
pending. No solver work is introduced on the GUI thread.

SN-017 Python/GDB/fixture control, profiles and numerical tolerances, MCU/toolchain
independence and PDF auto-open suppression/PID retries remain unchanged. SN-021
stays one Windows/local-NTFS managed document and explicit fixed E-01 replay with
inert Open. External overwrite, automatic executable resources, production Save,
general MCU/mixed-signal execution, power-loss durability, trusted origin and
redistribution permission remain unproven. SN-022 accepted only nine synthetic
process cases, nine layout checks and Qt-free headless worker execution;
real-engine integration and production IPC remain pending. SN-044 ownership
remains; January stabilization and February 2027 classroom use are planning targets.
