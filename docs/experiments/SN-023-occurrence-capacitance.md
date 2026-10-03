# SN-023: applied capacitance in one existing occurrence

Date: 2026-10-03. Status: implemented and tested for the owned fixture;
SN-023 remains in_progress. No new edit operation or electrical acceptance.

## Acceptance before implementation

The [peer inspector](SN-023-peer-navigation.md) selects the capacitor but previously
omitted its applied C value from the central caption. Show the already owned
`capacitance` parameter only with base unit `F`, including its immediate binding
origin. Use existing native literal C edit/history/create-only Save Copy to prove
that the caption follows applied state, not pending text or artwork. The
[summary](evidence/SN-023-occurrence-capacitance-summary.json) hashes the local
gate and independent original expectations recorded before production edits.

- Original `main/right/c` shows `0.000000220 F`, left `0.000001 F`, each with
  `containing-circuit:capacitance`. Keep full IDs, pin/local net, four drafts,
  Properties/library/analyzer and explicit capture independent.
- Existing right C `220 -> 470 nF` displays `0.000000470 F` after Apply; draft
  alone does not. Invalid edit, Undo/Redo, unchanged left, explicit copy/reopen
  and current default origin require bounded evidence. Wrong dimension receives
  no false F caption or enabled capacitance edit.
- Retain 67 existing controls and separately audit original R-only copy and
  C-only copy from fresh original input. Compare eleven stage/path/value/unit/
  origin/caption observations with the independent declared-JSON resolver; retain
  unchanged inputs/two one-SVG roots and inspect the final capacitor image.
- Build only the Qt editor locally in a fresh directory and reuse unchanged
  native/Preview/backend/worker/layout evidence by hashes. Preserve all 238 old
  evidence files/twelve overlays; gate task-only publication through checker,
  exact-source Foundation checks and authorized PR/squash/main workflow.

## Implemented and tested

Two production presentation lines append the owned resolved C string and origin
when the parameter ID/unit agree. No conversion, general property formatter,
application/domain API, schema, edit/history/save/engine/worker operation or Qt
headless dependency is introduced. The existing refresh requeries current graph
metadata. Applied declaration values are not measurements or model truth; a
capacitor remains inspectable without available artwork. No significant decision
is made, so no ADR is added.

PR #83 was reconfirmed MERGED before editing: source
`900aec95d1a404f1d4d904a78bfcebea9c74848f`, squash
`cbd27e2191abab47042fe560479d18b84a826853`, exact-source Foundation Windows
67/67 and Ubuntu 56/56 successful. Local/origin/remote main matched, with zero
unpublished commits and twelve overlays. Branch
`codex/sn-023-occurrence-capacitance` starts there.

- Fresh C++20 Release configure/editor build and [GUI01](evidence/SN-023-occurrence-capacitance-gui-01.json)
  passed first run with the installed Qt 6.11.1 MSVC x64 kit, MSVC 19.51.36246.0
  and Windows SDK 10.0.26100.0: 80 controls, retaining 67 and adding 13 C controls.
- Eleven independently resolved observations cover original right/left, pending
  draft, applied, undo, redo, left after right edit, reopen, wrong dimension,
  own default origin and final state. Values, units, immediate origins and exact
  caption lines agree with the Python declared-ID/decimal resolver.
- Separate saved-token audits prove R-only `2.2 -> 3.5 kohm` and C-only
  `220 -> 470 nF` copies, each from the original. Every other byte, original/
  variant input and two separate one-SVG roots stays unchanged. No multi-edit
  persistence assumption is introduced.
- Four drafts stay intact across left/right capacitor inspection; pending C
  does not change applied state or permit Save Copy/history. Applied C and pin/
  local-net IDs survive Undo/Redo; left remains unchanged. Invalid edit, existing
  copy and failed Open retain the current document/history/inspection. Successful
  Open clears selection/history and requires explicit occurrence/pin reselection.
- A valid seconds-dimension declaration omits the F caption and disables the
  existing C command. Removing only the component's C override exposes its own
  `default` origin, distinct from a containing-circuit value. These test-only
  variant Opens do not establish a user schema-edit or recovery flow.
- Review found no actionable defect. The final capacitor image shows the complete
  `0.000000470 F (containing-circuit:capacitance)` line, unavailable capacitor
  artwork, independent resistor library capture and Properties' literal origin.
  Existing R/peer fields and image remain a separate snapshot in the same report.
- Native sources/default CMake/main/helper/canvas and previous binaries stay
  hash-identical. Prior local native 150 requests, lifecycle 14 assertions/one
  audit and no-Qt execution/import evidence are reused with their original limits;
  no local native/backend/worker/layout matrix is repeated. Required Foundation
  results and final checker/squash/main checks are recorded by the source PR.
- No setup/build/runtime failure occurred in this slice. All earlier failures and
  inconclusive observations remain unchanged. Scripted images/layout settling
  are not human visible-desktop, keyboard/accessibility or physical DPI acceptance.

## Try the local build

From the repository root, choose a fresh output directory:

```text
python tools/run_editor_demo.py --editor build/sn023-occurrence-capacitance/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/capacitance-demo-01
```

Open the printed document and select `main/right/c` in **Occurrence (read only)**.
In **Structure**, select `main/right`, then change the existing capacitance field
to `470` and press **Apply Capacitance Value**. Return to Occurrence: its caption
shows `0.000000470 F`. Undo/Redo and Save Copy/explicit reopen use existing native
operations. Select `main/left/c` to compare the unchanged left value. Library
Preview and Properties remain independent; no capacitor artwork is promised.
This uses an installed kit and local build; packaging/human acceptance is pending.

## Pending and next bounded gate

Next review the usability of this existing fixture path before expanding editing:
bounded keyboard selection/navigation and explicit action focus are candidates
for a separate acceptance gate. Do not infer full accessibility from widget names
or shortcuts. Wiring/placement/general components/properties/history, instruments,
managed Save, real-engine workers/IPC and general execution remain pending.
Human dialogs/normal close/visible interaction, keyboard/accessibility, large
projects, physical monitor/DPI, cross-session layout and packaging remain pending.

Preserve SN-017 Python preparation/GDB/fixture control, accepted profiles/numerical
tolerances, MCU/toolchain independence and PDF/PID behavior. SN-021 stays one
Windows/local-NTFS managed document and explicit fixed E-01 replay with inert Open;
external overwrite, automatic executable resources, production save service,
general MCU/mixed-signal execution, power-loss durability, trusted origin and
redistribution permission remain unproven. SN-022 stays nine synthetic process/
nine layout cases and Qt-free worker execution with failed first setup/runtime
attempts retained; scripted retries are not a user recovery flow. Preserve
independent windows, adjustable panels and shared instrumentation ownership,
January stabilization and February 2027 classroom use as planning targets.
