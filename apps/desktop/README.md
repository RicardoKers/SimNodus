# Bounded Windows document editor

The bounded [SN-023 editor](../../docs/experiments/SN-023-document-editor.md) is a
native Qt Core/Gui/Widgets presentation over existing project acquisition,
name revision and create-only persistence. It is optional; the default build
and headless application tests require no Qt. No dependencies are downloaded.

It supports one inert declaration at a time: **File -> Open**, inspect declared
component definitions/pins and circuit instances/nets, change the top-level
display name with **Apply Name**, then **File -> Save Copy** using a new ASCII
leaf filename in the opened directory. Save Copy retains the original association
and dirty state; explicitly Open the copy to switch documents. Apply any pending
name/resistance text before saving. Open/exit asks before discarding pending or applied edits.
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
project/instance names and the literal resistance command. Only the most recent effective transition
is retained; a new applied edit replaces it and clears Redo. Invalid edits and
no-ops preserve it. Apply or restore all three pending text fields before using these
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

Components/Preview concerns a declared definition, with metadata only; Instance
Properties concerns a selected graph instance. Preview renders no symbol or model
and no resource is accessed. The central view is a read-only structural inspector,
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

Full placement/wiring/properties/multi-step undo, rendered symbols, production managed Save,
real worker/engine integration, shared instruments, physical monitor/DPI behavior,
cross-session layout and packaging remain pending. Document parsing and bounded
filesystem calls are synchronous; this slice measures no I/O responsiveness or
latency guarantee. No simulation is offered. See the [Qt inventory](../../docs/development/QT_INVENTORY.md)
for the still-open provenance, notices, runtime and binary redistribution gates.
