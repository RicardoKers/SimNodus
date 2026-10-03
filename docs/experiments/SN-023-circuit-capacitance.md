# SN-023: capacitance editing in Circuit Properties

Date: 2026-10-03. Status: implemented and tested for one existing containing-RC
literal value on the fixed canvas; SN-023 remains in_progress. No general editor
or simulation acceptance.

## Acceptance and implementation

The owner authorized continuation after the [fixed RC canvas](SN-023-rc-canvas.md).
The gate recorded before production/test edits is hashed in the
[summary](evidence/SN-023-circuit-capacitance-summary.json). One useful vertical
path moves the existing capacitance field into Circuit Properties through an
explicit **Edit Capacitance** action, while retaining the broader editor in
**Declaration Details**. It adds no domain/application operation, schema, quantity
parser, history stack, save authority, dependency or Qt module.

The actual existing field, limits label and Apply button share one Qt-owned panel.
Changing view transfers that panel, retaining the same widget, text and connection;
there is one C draft. Opening and read-only click inspection do not activate it.
Activation rechecks the supported closed RC shape, selected full C path and an
existing literal capacitance on its containing RC instance. The panel explicitly
identifies the containing target and fixed literal unit. No component override is
created. The original fixture's main/left C is inspectable but its containing
capacitance has no literal override, so compact editing is unavailable there.

Circuit Apply rechecks the current graph, selected C, canvas context, activation
target and native edit target before calling the existing native set_capacitance.
Resistor/context/unsupported/default selections hide and disable the compact
field while preserving its text. Returning to the retained matching C recovers
that draft; selecting a new editable containing target requires activation.
Changing targets refuses any pending instance name, R or C draft and retains the
project-name draft. Applying C preserves those other three fields. Details keeps
its existing editing semantics; Save Copy and one-step Undo/Redo remain explicit.

Components/Preview, independent editor/analyzer windows, adjustable panels and
shared instrumentation ownership remain unchanged. Canvas notation loads no
resources. Qt Core/Gui/Widgets remain presentation-only, with no engine work.

## Measured evidence

Pre-edit PR #85 was reconfirmed MERGED: source
bdcea7c2f9ec8e8e3c6a17060eaef67d88af9438, squash
64c30eb3d7a42fdbd2130485648fd2d298bd5141, exact-source Foundation Windows
67/67 and Ubuntu 56/56 successful (workflow 37131252915). Local/origin/live remote
main matched and no unpublished commits existed. The new branch is
codex/sn-023-circuit-capacitance. All 243 earlier evidence files/twelve independent
SN-045 overlays were backed up and hashed before implementation.

- Fresh Windows C++20 Release configure/editor build passed. No setup, build,
  runtime or evidence-helper attempt failed in this slice.
- [GUI01](evidence/SN-023-circuit-capacitance-gui-01.json) passed first run:
  41 controls and fourteen snapshots independently resolved from declared JSON.
  The separate Python driver audits exactly one main/right C token, 220 -> 470 nF,
  in the saved copy; all other bytes and input fixtures remain unchanged. Its root
  contains no resource files. Opening remains inert.
- The path covers explicit activation, draft versus applied values, five repeated
  transfers of the same widgets, four-draft retention, R/context stale-Apply
  refusal, zero/malformed C refusal, preservation of the other three drafts on C
  Apply, pending copy/history refusals, Undo/Redo, occupied copy/failed Open
  retention and explicit copy reopen/reactivation. Invalid editing preserves Redo.
- Three separately tested pending instance/R/C drafts refuse switching to an
  independently prepared editable left RC. With clean instance fields, activation
  changes target while preserving the project draft. The original left remains
  read-only. Missing parent literal refuses creation; reordered arrays/keys and
  equal HTML-looking labels retain full-ID targets; default component binding
  and wrong dimensions refuse compact editing.
- Active, draft and final images were inspected: target/unit/limits/field/Apply
  are legible, and a 470 draft leaves the 220 applied canvas unchanged until Apply.
  Scripted mouse events, widget transfer and captures do not establish physical
  mouse, human dialogs/close/recovery, keyboard/accessibility or monitor/DPI use.
- The unchanged [33-control canvas path](evidence/SN-023-circuit-capacitance-canvas-01.json)
  passed on the final binary: eighteen independent snapshots, five shape refusals,
  one C-byte audit, unchanged inputs and no resources.
- The unchanged [80-control occurrence path](evidence/SN-023-circuit-capacitance-occurrence-01.json)
  passed on the final binary: eleven independent C observations, two separate R/C
  byte audits, unchanged inputs and two one-SVG explicit roots. These regressions
  exercise changed widget ownership/Apply routing, not every old GUI matrix.
- Twenty-two sources/final binary are frozen; nineteen prior source/binary entries
  and fourteen earlier mixed native/source/binary artifacts match reuse hashes.
  Domain/application/schema/save/history/worker/backend sources are unchanged.
  Native 150 requests, lifecycle 14 assertions/one audit, no-Qt, Preview, backend,
  worker and layout evidence are reused without local native/backend matrices.
  Source PR records checker, exact-source required checks, squash and final main.

No significant architecture decision is selected; no ADR is added. All earlier
positive, negative and inconclusive evidence remains byte-preserved, including
SN-022's first setup/runtime failures and the previous canvas helper failures.
Shared planning publication includes only this task's prefix/row, excluding all
twelve independent SN-045 overlays.

## Try the local build

Use the installed Qt kit and this local development executable with a fresh root:

```text
python tools/run_editor_demo.py --editor build/sn023-circuit-capacitance/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/circuit-capacitance-demo-01
```

1. File -> Open the printed document/original.json. The right RC appears.
2. Click C, then Edit Capacitance. Properties identifies containing main/right
   and nF; the field starts at 220. The canvas remains visible.
3. Type 470; the applied diagram remains 220 nF until Apply Capacitance Value.
   Apply updates its base-value label to 0.000000470 F.
4. Try 0 and Apply: refusal retains the last applied value and pending 0 text.
   Restore 470 before Undo/Redo or Save Copy. Undo/Redo changes 220/470 nF.
5. Save Copy to a new leaf such as circuit-C-test.json. Explicitly Open the copy,
   confirming discard of the preceding document if appropriate; click C and
   Edit Capacitance again to inspect its persisted 470 nF value.

The older generic helper Preview/name/R instructions use Declaration Details.
This requires a local development kit, not a packaged release. Earlier demo
roots/processes are retained; a new helper root avoids overwriting user work.

## Proposed next gate and pending behavior

Gather visual feedback, then separately gate bounded engineering-unit captions
for this R/C fixture while preserving exact stored/applied values. General field
editing/library/Preview, positions/rotation/wiring/pan/zoom/arbitrary shapes,
instruments, managed Save, real-engine workers/production IPC/general execution,
large-project/human keyboard/accessibility/monitor/session/packaging stay pending.

Preserve SN-017 Python preparation/GDB/fixture control, accepted profiles and
tolerances, MCU/toolchain independence and PDF/PID behavior. SN-021 stays one
Windows/local-NTFS managed document and explicit fixed E-01 replay with inert Open;
external overwrite, executable-resource automation, production Save, general
MCU/mixed-signal, power-loss durability, trusted origin and redistribution remain
unproven. SN-022 only accepted nine synthetic process cases, nine layout checks
and a Qt-free headless worker; scripted retries are not human recovery proof.
SN-044 independent windows/panels/shared instrumentation and January stabilization/
February 2027 classroom use remain planning targets and constraints.
