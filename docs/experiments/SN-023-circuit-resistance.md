# SN-023: resistance editing in Circuit Properties

Date: 2026-10-05. Status: implemented and tested for existing containing-RC
literal resistance on the fixed canvas; SN-023 remains in_progress. General
component editing, wiring and desktop simulation remain pending.

## Acceptance and implementation

The owner liked the preceding exact-caption increment and authorized continuation.
This is informal positive feedback, not complete human usability acceptance.
The preimplementation gate is hashed in the
[summary](evidence/SN-023-circuit-resistance-summary.json).

Clicking a supported R inspects its current applied value. **Edit Resistance**
explicitly activates the existing literal resistance of its containing RC
instance. The original limits, field and Apply button now share one Qt-owned
panel transferred between Circuit and Declaration Details. There is one R field,
one connection and one R draft; no text copy or second native edit operation.
The field keeps the containing literal's fixed unit (kohm in this fixture).
The diagram uses exact engineering captions and Properties retains Base value.
Typing 3.5 leaves applied 2.2 kΩ unchanged until **Apply Resistance Value**.

Activation and Apply recheck the current graph, closed supported RC shape, full
R path, context, activation target and selected native containing target. C or
different-context inspection hides/disables compact R while retaining its draft.
Returning to the retained matching R recovers it. Changing to another editable
containing target requires explicit activation and refuses pending instance
name/R/C drafts separately; a clean switch preserves the project-name draft.
Missing parent literals and unsupported shapes/bindings stay read-only and never
create an override. The original left R has its own 1 kohm literal, so clean
explicit left activation is supported independently of right R.

R and C retain independent panels, activation identities and drafts on their
shared native containing target. Selecting one hides the other field; opposite
Apply refuses. Applying R preserves pending project/instance/C text. Existing
native one-step Undo/Redo and create-only Save Copy stay explicit. Successful
Open clears both activations/drafts/history; failed Open retains current state.
Opening and inspection do not access resources or run simulation.

No domain/application/schema/parser/history/save/worker/kernel change, dependency,
Qt module or instrumentation owner is added. Qt Core/Gui/Widgets remain in
presentation. Components/Preview, independent Circuit Editor and Signal Analyzer
windows, adjustable panels and shared instrumentation ownership are retained.

## Measured evidence

Pre-edit PR #87 was reconfirmed MERGED: source
4741468c823791770f379d66e43f15d20df9f558, squash
061e7816e575a8a6b4281a27529ae6a686157fc9. Exact-source Foundation Windows 67/67
and Ubuntu 56/56 passed in workflow 37297641695. Local/origin/live remote main
matched, with zero unpublished commits. Branch codex/sn-023-circuit-resistance
preserves 250 historical evidence files and twelve independent local overlays.

- Fresh Windows C++20 Release configure/editor build passed first run using
  Qt 6.11.1, native Windows platform and Fusion style.
- [R GUI01](evidence/SN-023-circuit-resistance-gui-01.json) passed first run:
  43 controls, fourteen independently source-resolved declared snapshots, one
  exact right R-token saved-byte audit (2.2 -> 3.5), 26 component captions and
  twelve Properties/base-value blocks. The [independent audit record](evidence/SN-023-circuit-resistance-audit-01.json)
  retains the source-derived checks, caption oracle results and process status.
  All other copy bytes, original/variant inputs and fixture bytes stayed unchanged;
  the document root had no resources. Left remained independently 1000 ohm.
- R coverage includes five same-widget transfers/four drafts, opposite C
  activation/Apply guards, context/target/pending-field refusals, invalid and
  malformed values, Redo preservation, native Apply/Undo/Redo, explicit copy/
  occupied copy/failed Open/reopen, missing parent, reordered IDs and HTML labels.
  The reused default-binding and wrong-dimension variants invalidate the closed
  RC shape through C; these are whole-shape refusal tests, not specific R-unit
  or general unit-entry acceptance.
- The unchanged [C/caption driver](evidence/SN-023-circuit-resistance-caption-01.json)
  passed first run on the final binary: 41 C controls, fourteen independent
  snapshots, a separate one C-token audit and unchanged inputs/no resources.
  Its [caption audit](evidence/SN-023-circuit-resistance-caption-audit-01.json)
  passed 29 helper cases, 26 component captions and twelve Properties blocks.
  This focused regression covers changed widget ownership/Apply routing.
- R active/draft/final and C final images were inspected. Units, exact base
  values, targets, limits, fields and Apply controls are legible in the normal
  fixture; R draft 3.5 leaves applied 2.2 kΩ, and R final shows 3.5 kΩ/3500 ohm.
  Three R and four C images are retained. Scripted events/captures do not prove
  human dialogs/recovery, physical mouse, keyboard/accessibility, monitor/DPI,
  cross-session layout, arbitrary long-label behavior or packaging.
- An ignored acceptance-generation helper failed a marker-count assertion before
  writing the new method; the diagnostic was retained and the expectation fixed.
  No production file changed in that failed helper attempt. No configure, build
  or runtime attempt failed. All earlier positives/negatives/inconclusive evidence,
  including SN-022's first setup/runtime failures, remain byte-preserved. A
  scripted helper retry is not a tested human recovery flow.
- Twenty-four sources/final binary are frozen; 21 prior source/binary entries,
  fourteen native artifacts and three unchanged C/native method sections match
  reuse hashes. Canvas geometry/resolver/selection/caption code is unchanged.
  Prior canvas 33/18 and occurrence 80/11/two audits, native 150 requests,
  lifecycle 14 assertions/one audit, no-Qt, Preview/backend/worker/layout evidence
  are reused without local matrix reruns. The source PR records the repository
  checker, exact task-only checker, required exact-source Foundation checks,
  guarded squash and final main confirmation.

No significant architecture decision is selected; no ADR is added. Shared
planning publication contains only this task's prefix and SN-023 row, excluding
all twelve independent SN-045 overlays.

## Try the local build

Use the installed development kit and a fresh output root:

```text
python tools/run_editor_demo.py --editor build/sn023-circuit-resistance/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/circuit-resistance-demo-01
```

1. File -> Open the printed document/original.json. Click right R: inspect
   2.2 kΩ and Base value 2200 ohm without activating an edit.
2. Click Edit Resistance, type 3.5 in the fixed kohm field. The diagram remains
   2.2 kΩ until Apply Resistance Value, then shows 3.5 kΩ/Base value 3500 ohm.
3. Click C, Edit Capacitance and apply 470 nF. R stays 3.5 kΩ. Each component's
   applied caption and independent field are shown when selected. Restore or
   apply any pending field text before history/copy or a different-target edit.
4. Save Copy to a new leaf such as RC-test.json, explicitly Open the copy and
   click R/C to inspect the persisted values; activate each field explicitly.
5. Choose RC instance: left and click R. With no pending instance fields,
   Edit Resistance explicitly activates its independent 1 kohm literal. Original
   left C remains inspectable at 1 µF without a compact literal edit.

The optional START_CIRCUIT_RESISTANCE_DEMO.cmd is retained under
build/sn023-circuit-resistance. The development executable requires the installed
kit; packaging is pending. Older demo roots/processes/user files remain intact.
Use another fresh output root if the proposed root already exists.

## Proposed next gate and pending behavior

Gather feedback, then separately gate bounded fit/zoom navigation on this fixed
canvas. This is proposed only; no stored positions or general component/wiring
editor is selected here. Component/library/Preview properties, placement,
rotation/wiring/pan/arbitrary shapes, instruments, managed Save, real-engine
workers/production IPC/general execution and human usability/packaging stay pending.

SN-017 Python/GDB/fixture control, accepted profiles/numerical tolerances,
MCU/toolchain independence and PDF auto-open suppression/PID retries stay unchanged.
SN-021 stays one Windows/local-NTFS managed document and explicit fixed E-01
replay with inert Open. External overwrite, automatic executable resources,
production Save, general MCU/mixed-signal execution, power-loss durability,
trusted origin and redistribution permission remain unproven. SN-022 accepted
only nine synthetic process cases, nine layout checks and Qt-free headless worker
execution; real-engine integration/production IPC remain pending, and scripted
retries do not prove user recovery. SN-044 ownership remains; January stabilization
and February 2027 classroom use remain planning targets.
