# Bounded Windows document editor

The first [SN-023 slice](../../docs/experiments/SN-023-document-editor.md) is a
native Qt Core/Gui/Widgets presentation over existing project acquisition,
name revision and create-only persistence. It is optional; the default build
and headless application tests require no Qt. No dependencies are downloaded.

It supports one inert declaration at a time: **File -> Open**, inspect declared
component definitions/pins and circuit instances/nets, change the top-level
display name with **Apply Name**, then **File -> Save Copy** using a new ASCII
leaf filename in the opened directory. Save Copy retains the original association
and dirty state; explicitly Open the copy to switch documents. Apply any pending
name text before saving. Open/exit asks before discarding pending or applied edits.
Failures retain the current document and expose native diagnostics.

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
```

The GUI script uses a byte-identical `two-rc-project.json` and a separate derived
fixture with an HTML-like instance name. Both lack referenced resources. It runs
23 assertions per fixture and independent exact-byte/name audits, saving reports,
stdout/stderr and editor screenshots. It invokes shared application commands,
selection/button signals, discard-dialog Cancel and panel controls. Native file
dialogs, discard approval, keyboard/accessibility and human recovery are unverified.
This is distinct from SN-022's synthetic worker/retry matrix, which is unchanged.

Full placement/wiring/properties/undo, rendered symbols, production managed Save,
real worker/engine integration, shared instruments, physical monitor/DPI behavior,
cross-session layout and packaging remain pending. Document parsing and bounded
filesystem calls are synchronous; this slice measures no I/O responsiveness or
latency guarantee. No simulation is offered. See the [Qt inventory](../../docs/development/QT_INVENTORY.md)
for the still-open provenance, notices, runtime and binary redistribution gates.
