# SN-023: fixed declared RC circuit canvas

Date: 2026-10-03. Status: implemented and tested for the owned closed RC shape;
SN-023 remains in_progress. No general schematic editor or simulation acceptance.

## Acceptance and owner direction

The owner found the declaration/inspection window far from the intended simulator
interface and supplied a QSpice screenshot as a conceptual layout reference:
large clear schematic area, component browser/Preview at left, selected-instance
Properties at right, compact actions. This changes the next-step priority from
keyboard refinements to a useful visual circuit path. It preserves
[SN-044](../architecture/DESKTOP_UX.md), independent editor/analyzer windows,
adjustable panels and shared instrumentation ownership. The owner's apparent
presence of earlier controls is informal feedback, not a usability pass.
No QSpice image, artwork, code, model, logo or dependency is incorporated.

The [summary](evidence/SN-023-rc-canvas-summary.json) hashes the gate recorded before
production/test edits. The first canvas is one selected RC occurrence, initially
main/right with explicit main/left context: own R/C notation, three exact local
nets, input/output/reference ports and disposable fixed coordinates. No voltage
source, inferred ground, hierarchy flattening or saved geometry is introduced.
The shape must match component/pin/net/port IDs, quantities, dimensions and exact
endpoint-kind/path sets; names and ordering do not establish identity.

Static review before first canvas runtime found that literal/default component
bindings could still receive a containing-RC edit action, although changing that
parent parameter would not change the component. The gate was reduced to exact
containing-circuit:resistance/capacitance bindings. A separately retained gate
refinement and valid default-binding refusal record this correction. The original
preimplementation gate is unchanged; no new native binding operation is added.

## Implemented and tested

The Qt-only RcCanvas uses existing owned occurrence/local-net inspection. Its
built-in diagram notation is independent of project SVG resources or symbol
pin maps; it is not a generic symbol renderer or proof of model behavior.
Painting and hit testing share a fitted transform. A click rechecks the current
owned graph and selects the existing full-ID occurrence without changing the
native edit target, four drafts, document bytes/history or library capture.
Unsupported shapes clear diagram/selection and offer Details instead of false
fixed wires. Open remains inert: no referenced resources or engines are accessed.

Circuit is the default central view. The right dock shows compact read-only
component information and an explicit **Edit containing RC instance...** action.
That action identifies/selects the existing containing instance and opens the
previous editor in **Declaration Details**. Pending fields refuse replacement
of a different edit target. Native Apply, one-step Undo/Redo and create-only
Save Copy are reused. No component override is created or binding silently changed.
Changing view retains Qt-owned editor widgets and drafts. Components/explicit
native Preview remain independent and unchanged; its general library UI/Preview
is still pending. The independent analyzer remains empty.

Pre-edit PR #84 was reconfirmed MERGED: source
61c13c471752862dccab119d3fd8b686e93ec7a1, squash
84a046a8d5a4d06fe077848cde79dd5d47939a58; exact-source Foundation Windows
67/67 and Ubuntu 56/56 successful. Local/origin/remote main matched, no unpublished
commits; branch codex/sn-023-rc-canvas starts there.

- Fresh Windows C++20 Release configure and three editor build iterations passed.
  Build 02 added a visibility assertion; build 03 included the reviewed binding
  guard before first canvas runtime. No Qt setup/build/runtime failure occurred.
- [Canvas GUI01](evidence/SN-023-rc-canvas-gui-01.json) passed first run: 33 controls,
  eighteen snapshots independently resolved from declared JSON and exact one-C-token
  saved-byte audit. Inputs are unchanged, with no resource files in the test root.
- The path covers right R/C click, empty click, resize hit testing, explicit parent
  edit, four drafts across click/context, pending copy/history refusals, existing
  C 220 -> 470 nF Apply/invalid edit/Undo/Redo, independent left C, create-only copy,
  occupied-copy/invalid-Open retention and explicit reopen/reselection.
- Reordered arrays/keys, equal HTML-looking labels and swapped project symbol maps
  preserve identities. Missing peer endpoint, changed net ID, extra logical pin,
  wrong C dimension and a nonforwarded/default C refuse the closed diagram.
  Five refusal snapshots contain no stale components, nets or selection.
- Initial/final images inspected: own resistor/capacitor symbols, three open ports,
  complete normal-fixture labels, C selection highlight, 220/470 nF base values,
  compact read-only Properties and visible explicit containing-edit action.
  Scripted mouse events/resize/layout settling are not physical mouse, DPI,
  accessibility, human dialog/close/recovery or classroom usability acceptance.
- The unchanged [80-control occurrence path](evidence/SN-023-rc-canvas-regression-02.json)
  passed on the final binary after the binding guard, with eleven independent C
  observations/two separate R/C persisted-byte audits and unchanged original/
  variant inputs/two single-SVG roots. Its first run also passed on build 01;
  both raw runs remain separate. Old harnesses explicitly use Details; only this
  occurrence path is rerun, not every prior GUI matrix.
- The local snapshot helper's first attempt failed an assertion because an absolute
  binary path reset its destination. Existing source snapshots were retained;
  normalization/resumption required identical bytes and succeeded. This operational
  failure is retained and does not establish user recovery. Twenty-one current
  source files/final binary are frozen; prior sources/binaries match reuse hashes.
- The first evidence-helper check refused the intentionally changed Qt main.cpp
  in a historical mixed source/binary bundle. Its failure was retained; only that
  authorized presentation entry was excluded from reuse, and all fourteen remaining
  bundle artifacts match. No prior native result is relabeled or rerun.
- Domain/application/schema/save/history/engine/worker/default headless build,
  native sources and previous binaries remain unchanged. Prior native 150 requests,
  lifecycle 14 assertions/one audit, no-Qt/Preview/backend/worker/layout evidence
  are reused by hashes. Required exact-head Foundation/checker/task-only publication,
  squash and final main results are recorded by the source PR.

No significant architecture decision is selected; no ADR is added. All 240 earlier
evidence files and twelve independent SN-045 overlays are retained. Shared planning
files publish only this task's prefix/row, never independent overlay text.

## Try the local build

From the repository root, use a new output directory:

```text
python tools/run_editor_demo.py --editor build/sn023-rc-canvas/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/rc-canvas-demo-01
```

1. File -> Open the printed document/original.json. The right RC appears in Circuit.
2. Click the resistor or capacitor; the right dock shows the selected applied value.
3. For right C, click Edit containing RC instance..., change C from 220 to 470 nF,
   then Apply Capacitance Value. Toggle Declaration Details off to return Circuit.
4. Edit -> Undo Last Edit / Redo Last Edit updates the diagram. Choose RC instance:
   left to compare its unchanged 1 uF C, then return right.
5. File -> Save Copy with a new leaf filename, e.g. canvas-test.json. Explicitly Open
   that copy, confirm discarding the preceding in-memory document if appropriate,
   then click C again. Its applied value remains 470 nF; earlier history is cleared.

This requires the installed Qt kit/local development build, not a packaged release.
The helper's generic Preview/name/R walkthrough is available through Details;
resource Preview remains explicit and is not needed for built-in canvas notation.

## Pending and next bounded gate

Gather human feedback on this visual path, then separately gate one compact C edit
in Circuit Properties with explicit containing-instance ownership, existing native
operations and draft/history/copy evidence. Do not expand to the whole component
editor/wiring system/instruments. General library/Preview, engineering-unit labels,
free placement/rotation/wiring/pan/zoom, arbitrary shapes/long-label/large-project
layout, multi-step history, instruments, managed Save, real-engine workers/IPC and
general execution remain pending. Keyboard/accessibility, physical monitor/DPI,
cross-session layout, human dialogs/normal close/recovery and packaging are pending.

Preserve SN-017 Python/GDB/fixture control, profiles/tolerances, MCU/toolchain
independence and PDF/PID behavior. SN-021 stays one Windows/local-NTFS managed
document and explicit fixed E-01 replay, with inert Open; external overwrite,
automatic executable resources, production Save, general MCU/mixed-signal,
power-loss durability, trusted origin and redistribution remain unproven.
SN-022 stays nine synthetic process/nine layout cases and Qt-free worker execution,
including failed first setup/runtime attempts; scripted retries are not human
recovery acceptance. January stabilization and February 2027 classroom use remain
planning targets, not proof of feasibility.
