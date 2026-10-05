# Bounded Windows document editor

[Exact RC captions](../../docs/experiments/SN-023-rc-captions.md) show 2.2 kΩ,
220/470 nF and 1 µF without rounding or changing document values. Circuit Properties
also shows the exact Base value. The compact C field keeps its displayed fixed
unit; pending text leaves applied captions unchanged until explicit Apply.

[Compact C editing](../../docs/experiments/SN-023-circuit-capacitance.md) is available
without leaving the canvas: click right C, then **Edit Capacitance**, type a value
in the displayed fixed unit and use **Apply Capacitance Value**. The target is the
containing RC literal. The same field/draft is retained in Details. Default/missing
parent literals and unsupported shapes stay read-only; Save Copy remains explicit.

The default [fixed RC canvas](../../docs/experiments/SN-023-rc-canvas.md) shows one
right/left occurrence with own built-in R/C notation, exact declared local nets
and three ports. Click a component to inspect it; **Edit containing RC instance...**
opens the existing native editor in **Declaration Details**. The forwarded values
belong to that containing instance; no component override is created. Toggle
**View -> Declaration Details** off to return Circuit. Unsupported shapes show a
placeholder; this is not a general renderer, ground/source inference or simulation.
Component library/explicit Preview remains independent; built-in canvas notation
loads no SVG/resource. The following older declaration workflows use Details.

The bounded [SN-023 editor](../../docs/experiments/SN-023-document-editor.md) is a
native Qt Core/Gui/Widgets presentation over existing project acquisition,
name revision and create-only persistence. It is optional; the default build
and headless application tests require no Qt. No dependencies are downloaded.

For a runnable owned-fixture walkthrough, use the
[interactive development demo](../../docs/experiments/SN-023-interactive-demo.md):

```text
python tools/run_editor_demo.py --editor build/sn023-pin-preview/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/editor-demo-01
```

Adjust the locally built executable/installed kit paths and choose a new output
directory. The helper prepares separate document/artwork roots, prints explicit
Open/Preview/edit/Save Copy instructions and starts the unchanged blank window.
It waits for the editor to close and retains diagnostics; it does not validate
window visibility. `--prepare-only --out NEW_DIR` copies inputs without Qt.
No executable, Qt runtime or model is copied or downloaded; this is not packaging.

The [read-only occurrence tab](../../docs/experiments/SN-023-occurrence-view.md)
offers full existing component paths independently of Properties/library Preview.
Select `main/right/r`, then **Capture Occurrence Artwork...** and explicitly choose
an artwork root. It captures only the previous owned resistor SVG; other artwork
remains unavailable. The caption shows current name/applied resistance, not a
measurement or saved position. Switch to Structure and edit `main/right` to `3.5`
kohm: the central path/capture survives and displays `3500 ohm`. Changed occurrence
paths clear that capture, library choices leave it independent, successful Open
clears it and failures retain it. Re-select/capture explicitly after reopening a copy.
The demo helper's optional occurrence instructions work with the latest local build:

```text
python tools/run_editor_demo.py --editor build/sn023-rc-captions/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/rc-captions-demo-01
```

The [local terminal table](../../docs/experiments/SN-023-terminal-membership.md)
works without artwork capture: `main/right/r` pin `p` shows `main/right/drive`,
and `n` shows `main/right/junction`. Pin/net names are plain display text; full
IDs retain the containing occurrence. Unlisted pins show **Unconnected locally**.
This is only declared local membership, without hierarchy flattening, a connection
edit or physical/model truth. Existing edit/history/copy requery the current graph;
Properties/library selection remains independent.

Select a pin row to [inspect all direct members](../../docs/experiments/SN-023-local-endpoints.md)
of its local net. For `main/right/r` pin `n`, the second table lists local port
`main/right/output` and component pins `main/right/c/p`, `main/right/r/n`.
Explicit kinds distinguish local ports, component pins and subcircuit ports;
ports are not traversed. Missing membership shows **Unconnected locally**.
The full occurrence/pin ID persists through existing edit/history and clears on
missing pin/path or successful Open. No implicit selection/capture or net edit.

Select `main/right/c/p` in that table and press **Inspect Selected Component**
to [navigate explicitly to the declared peer](../../docs/experiments/SN-023-peer-navigation.md).
The action uses kind/full IDs and rechecks current membership; ports, self and
stale rows refuse. It preserves four drafts and independent Properties/library
selection, while clearing the old occurrence capture/pin/details. Select a pin
and request any new capture explicitly. Returning via capacitor pin `p` and
endpoint `main/right/r/n` requires a fresh explicit capture of the resistor.

The [applied capacitance caption](../../docs/experiments/SN-023-occurrence-capacitance.md)
shows `main/right/c` as `0.000000220 F (containing-circuit:capacitance)`.
Use the existing `main/right` Properties C field to apply `470 nF`; the selected
capacitor displays `0.000000470 F`. Pending text is separate, Undo/Redo requery
current metadata, and Save Copy/reopen requires explicit reselection. Left C stays
`0.000001 F`. This is an applied declaration, with no measurement or capacitor
artwork claim; a wrong dimension does not receive an F label.

It supports one inert declaration at a time: **File -> Open**, inspect declared
component definitions/pins and circuit instances/nets, change the top-level
display name with **Apply Name**, then **File -> Save Copy** using a new ASCII
leaf filename in the opened directory. Save Copy retains the original association
and dirty state; explicitly Open the copy to switch documents. Apply any pending
name/R/C text before saving. Open/exit asks before discarding pending or applied edits.
Failures retain the current document and expose native diagnostics.

The [next tested increment](../../docs/experiments/SN-023-instance-label.md) adds
**Declared instance display name -> Apply Instance Name** in Instance Properties.
Selection resolves circuit-definition ID plus instance ID; the name is shared by
all occurrences of that definition and changes no ID, connection or parameter.
The field is disabled for circuit/net rows and no selection. Pending instance
text blocks Save Copy, and changing selection offers explicit Discard or Cancel.
Cancel retains the selected IDs and draft. Failed edits retain the graph and text.

The [one-step history increment](../../docs/experiments/SN-023-name-history.md)
originally added history for applied names. The current menu is
**Edit -> Undo Last Edit (one step)** / **Redo Last Edit (one step)** for applied
project/instance names and the literal R/C commands. Only the most recent effective transition
is retained; a new applied edit replaces it and clears Redo. Invalid edits and
no-ops preserve it. Apply or restore all four pending text fields before using these
actions. Restoring an edit retains selected IDs and Preview. Save Copy preserves
history; successful Open clears it and starts clean. Document keyboard shortcuts
and multi-step/general undo are pending; typing is not application history.

The [literal resistance increment](../../docs/experiments/SN-023-resistance-value.md)
adds **Apply Resistance Value** to Instance Properties for an existing literal
`resistance` override. `main/left` in the owned fixture starts at `1 kohm`; the
panel displays its fixed input unit and target bounds `100` through `10000 ohm`.
The native validator enforces exact decimal syntax and inclusive limits. No
unit/default/forwarded binding edit or arbitrary parameter field is offered.
This changes a selected source declaration and its occurrences. Applying one
field retains other drafts. Selection Cancel retains both instance drafts;
Discard clears both while preserving the project draft. Equal decoded text is a
no-op; another valid spelling of the same magnitude is a real byte edit.

The [capacitance increment](../../docs/experiments/SN-023-capacitance-value.md)
adds **Apply Capacitance Value** for an existing literal `capacitance` override
in fixed F/uF/nF. The owned fixture's `main/right` starts at `220 nF` with target
bounds `0.000000001` through `0.001 F`. Applying any field retains the other three
drafts. Selection Cancel retains all; Discard clears the three local instance
drafts while retaining project text. No override is created or converted.

The [read-only R/C Inspector](../../docs/experiments/SN-023-effective-parameters.md)
offers full root-prefixed occurrence paths for reused source definitions, showing
applied values in base ohm/F and immediate literal/default/containing-circuit
origins. It does not show measurements or pending drafts. Selection is inert;
Apply/Undo/Redo refresh from the current graph and retain the occurrence path.
Properties content scrolls so existing controls remain reachable in a short panel.
The final scripted path passed 33 controls and two byte audits; the failed first
Preview assertion is retained. Actual reused-path selection has two active name
drafts in the fixture; four active fields were checked separately on one path.

Build `native_inspection_probe`, `editor_inspection_tests` and
`editor_document_tests` for the three focused CTests. Use a fresh output directory:

```text
python tests/resources/editor_inspection_acceptance.py --editor build/sn023-effective-parameters/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-effective-parameters/new-gui-run
```

The [explicit fixture Preview](../../docs/experiments/SN-023-fixture-preview.md)
supports only the owned fixture's `resistor` definition and unchanged `passive.svg`.
Select it under Components, then **Preview Fixture Artwork...** and explicitly
choose the resource root containing `tests/schema/fixtures/assets/passive.svg`
(the repository root for this original fixture). Open and selection access no
resources. Preview verifies/captures only this one file, with model/LICENSE files
allowed to remain absent. Other components/artwork remain unavailable. The caption
reports captured identity/bytes/hash and the available closed fixture convention.
Failed recapture retains the prior artifact; repaint/resize and name/R/C/history
reuse immutable captured bytes. Catalog changes and successful Open clear it.
The resource root is independent of the opened document's directory and grants
no global verification or execution authority. Native directory-dialog/human
usability remains pending; the scripted path calls the same operation directly.

The [declared pin increment](../../docs/experiments/SN-023-pin-preview.md) maps
logical pins through the project's explicit map to a private fixture convention:
`two-pin/a` is the left lead, `two-pin/b` the right. The original shows `p/a` and
`n/b`; a swapped declared map shows `n/a` and `p/b`. Markers use the same fitted
view; captions retain full IDs while drawn labels may be elided. The SVG declares
no anchors and this convention establishes no model/electrical truth or general
anchor API. Unknown descriptors/anchor counts leave artwork visible with anchors
unavailable. The complete owned map governs retention, even in that unavailable
case; source ordering and labels do not establish identity. Minimum panel sizing
keeps wrapped captions readable. The final 20-control path and one persisted-byte
audit passed; the first clipped caption and second layout-settling failure remain
retained. Neither scripted layout settling nor retries establish human recovery.

```text
python tests/resources/editor_pin_preview_acceptance.py --editor build/sn023-pin-preview/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-pin-preview/new-gui-run
```

Build `native_preview_probe` and `editor_document_tests` for the two focused CTests.
Use a fresh GUI output directory:

```text
python tests/resources/editor_preview_acceptance.py --editor build/sn023-fixture-preview/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-fixture-preview/new-gui-run
```

Components/Preview concerns a declared definition; Instance Properties concerns
a selected graph instance. The central view is a read-only structural inspector,
not a schematic canvas. **View -> Open Signal Analyzer** opens the independent
window; shared instrumentation is pending and no measurement data is fabricated.
Docks/splitter are adjustable and panels can be hidden/reopened through View.
Workspace state is not stored between sessions.

## Local build and interactive run

Use the existing installed kit and MSVC toolchain; no installation or distribution
is implied. From the repository root in PowerShell:

```powershell
$cmake = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' $cmake -S . -B build/sn023/native-build -G 'Visual Studio 18 2026' -A x64 -DSIMNODUS_BUILD_DESKTOP=ON '-DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64'
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' $cmake --build build/sn023/native-build --config Release --target simnodus_document_editor editor_document_tests
$env:PATH = 'C:/Qt/6.11.1/msvc2022_64/bin;' + $env:PATH
$env:QT_QPA_PLATFORM = 'windows'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:/Qt/6.11.1/msvc2022_64/plugins/platforms'
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' build/sn023/native-build/apps/desktop/Release/simnodus_document_editor.exe
```

The Python wrapper normalizes duplicate inherited PATH/Path keys on the measured
host, as in SN-022. The editor is native C++20. Existing Windows/local-NTFS root
and ASCII leaf restrictions apply. Retaining the acquisition-directory spelling
does not verify declared resource roots or hold a physical directory lease.

## Repeatable acceptance

Choose a new output directory for every GUI run:

```powershell
python tests/resources/editor_document_regression.py --probe build/sn023/native-build/Release/editor_document_tests.exe
python tests/resources/editor_desktop_acceptance.py --editor build/sn023/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023/new-gui-run
python tests/resources/editor_instance_acceptance.py --editor build/sn023/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023/new-instance-run
python tests/resources/editor_name_history_regression.py --probe build/sn023-name-history/native-build/Release/editor_name_history_tests.exe
python tests/resources/editor_history_acceptance.py --editor build/sn023-name-history/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-name-history/new-gui-run
python tests/resources/editor_resistance_regression.py --probe build/sn023-resistance-value/native-build/Release/editor_resistance_tests.exe
python tests/resources/editor_resistance_acceptance.py --editor build/sn023-resistance-value/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-resistance-value/new-gui-run
python tests/resources/editor_capacitance_regression.py --probe build/sn023-capacitance-value/native-build/Release/editor_capacitance_tests.exe
python tests/resources/editor_capacitance_acceptance.py --editor build/sn023-capacitance-value/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-capacitance-value/new-gui-run
```

The original GUI script uses a byte-identical `two-rc-project.json` and a separate derived
fixture with an HTML-like instance name. Both lack referenced resources. It runs
23 assertions per fixture and independent exact-byte/name audits, saving reports,
stdout/stderr and editor screenshots. It invokes shared application commands,
selection/button signals, discard-dialog Cancel and panel controls. Native file
dialogs, discard approval, keyboard/accessibility and human recovery are unverified.
This is distinct from SN-022's synthetic worker/retry matrix, which is unchanged.

The dedicated instance script uses only the original fixture and adds 21 checks
for the selected field, definition edit, literal untrusted text, failed edits,
pending drafts, scripted Discard/Cancel and exact copy/reopen. Its independent
audit checks that only `rc/r`'s name token changed. The prior 23-check window/layout
evidence is reused; no old backend or layout matrix was rerun for this increment.
The first dedicated 20-check run is retained before adding the net-row check.

The dedicated history script checks 26 controls on the original fixture with two
persisted byte audits. The native history lifecycle also checks an owned escaped
name variant, with seven saved snapshot byte audits. It includes one-step limits,
branching, no-op/error retention, exact dirty states, draft guards and Open resets.
Earlier GUI/revision/backend matrices are reused. To build the new native probe,
use the current sources and the additional `editor_name_history_tests` target;
the measured build directory and commands are in the linked history report.

The dedicated resistance script passed 29 controls and two independent saved-byte
audits, including mixed name/value history and all three draft guards. Its native
lifecycle passed 23 assertions and six saved-byte audits. Seven pure revision
tests issue 53 requests against independent graph/source/effective-parameter and
byte expectations. Two failing Python-oracle attempts are retained in the report;
the first GUI/headless runs passed. Existing backend/layout evidence is reused.
Build the additional `editor_resistance_tests` and `native_revision_probe` targets
with the current sources; exact measured commands are in the linked report.

The dedicated capacitance script passed 31 controls and two saved-byte audits;
the native lifecycle passed 21 assertions and seven persisted snapshots. Six pure
tests issue 64 requests, including fixed F/uF/nF, narrowed selected-target bounds,
wrong dimensions and unaffected resistance/left subtree. Eight affected CTests
passed in the first run; GUI/headless first runs passed. Previous GUI/layout/backend
matrices are reused. Build the new `editor_capacitance_tests` target as well.

Full placement/wiring/properties/multi-step undo, general rendered symbols/anchors, production managed Save,
real worker/engine integration, shared instruments, physical monitor/DPI behavior,
cross-session layout and packaging remain pending. Document parsing and bounded
filesystem calls are synchronous; this slice measures no I/O responsiveness or
latency guarantee. No simulation is offered. See the [Qt inventory](../../docs/development/QT_INVENTORY.md)
for the still-open provenance, notices, runtime and binary redistribution gates.
