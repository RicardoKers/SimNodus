# SN-023: existing literal resistance value

Date: 2026-09-30. Status: implemented and tested for this closed field and mixed
one-step history. Full SN-023 remains in_progress.

## Bounded acceptance before implementation

Extend the [one-step name-history slice](SN-023-name-history.md) with one field:
the value string of an **existing literal `resistance` override**, selected by
circuit-definition ID and instance ID. The first path is `main/left` in the owned
`two-rc-project.json`: `1 kohm`, target range `100` through `10000 ohm` inclusive.
The same closed field can be offered on another eligible selected declaration.
This is a source declaration edit affecting that declaration's occurrences,
not an occurrence override or permission to simulate the revised project.

- A Qt-free native view reads the current value, fixed declared unit and target
  bounds in base units from an owned immutable validated graph. Borrowed strings
  are valid only while that graph is retained. Literal ohm/kohm overrides only:
  no creation, forwarded/default conversion, unit edit or arbitrary parameter ID.
- Fully validate the original first. Select by stable IDs and decoded immediate
  keys, replacing exactly the selected JSON `value` string token. Preserve every
  other byte, including unit spelling, other overrides, defaults, bindings, names,
  IDs, resources and temporal policy. Fully validate/rebuild the candidate graph,
  source map and effective parameters with the existing exact-decimal validator.
- Enforce its existing decimal grammar/precision/exponent, dimension and inclusive
  declared range, plus the 1 MiB document cap. Missing/forwarded/wrong-dimension
  selectors, invalid UTF-8/text/quantity, invalid base and oversized candidates
  refuse with structured errors without changing the current application state.
  Equal decoded value text is a no-op retaining original escape spelling; different
  valid numeric spellings are real byte edits even if their magnitudes are equal.
- Independent tests check exact unaffected bytes, complete graph/source spans and
  effective parameter resolution: changing `main/left` affects only its resistance
  and forwarded resistor descendant, leaving the right subtree and capacitance
  unchanged. Include reordered arrays/keys, escaped keys/values, duplicate labels,
  missing/forwarded bindings, inclusive/outside bounds and size ceilings.
- Names and this resistance command share the existing one-step snapshot history.
  Rename internal history APIs/menu wording from name edit to edit; do not retain
  a stale name snapshot that would accidentally discard a later resistance edit.
  Effective changes replace the one step; errors/no-ops preserve it. Exact undo,
  redo, dirty-baseline, Save Copy retention and successful Open reset still apply.
  No command framework, multi-step history or new authority is introduced.
- Instance Properties shows value, fixed input unit and declared target bounds in
  base units. Unsupported rows/bindings disable this field. Apply changes one field
  and preserves other pending text. All three pending fields block Save Copy and
  Undo/Redo; selected-instance name/resistance drafts guard selection changes,
  Open and exit. Scripted Cancel/Discard retains or explicitly discards drafts.
  Applying a name must not erase a resistance draft, nor vice versa.
- A dedicated native Windows control path uses actual Apply/Edit actions, both
  valid/invalid numeric input, other-field draft preservation, selection/Preview
  retention, scripted guards, exact copy/reopen and mixed one-step history. A
  headless lifecycle verifies the same application behavior and persistence.
  Remove Qt environment/kit PATH and inspect headless PE dependencies. Reuse old
  GUI/layout/backend evidence; repeat only directly affected native checks.
- Preserve all 210 earlier evidence files and twelve independent SN-045 overlays;
  update status/results, run repository and staged-publication checkers, then use
  the required branch/PR/Foundation/squash workflow for validated work.

Qt remains Core/Gui/Widgets presentation only. Keep editor/analyzer windows and
Preview versus instance Properties independent. No new asset/package or ADR is
planned for this bounded existing-schema command. Simulation remains unavailable.

## Implemented and measured

`literal_resistance` reads borrowed value/unit/target bounds from the retained
validated graph. `revise_resistance` selects the existing field by stable IDs,
replaces one token and delegates complete original/candidate validation to the
unchanged project loader. `EditorDocument::set_resistance` uses the same one-step
immutable snapshot as names; `undo_edit`/`redo_edit` replace name-specific APIs.
The default build and native tests need no Qt. Presentation re-resolves selected
IDs after Apply/restore; its three drafts are independent of applied graph state.

The existing installed Qt 6.11.1 MSVC x64 kit, C++20, Release, MSVC 19.51.36246.0,
Windows SDK 10.0.26100.0, native `windows` plugin and Fusion style were used.
Nothing was downloaded. Commands from the repository root:

```powershell
$cmake = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' $cmake -S . -B build/sn023-resistance-value/native-build -G 'Visual Studio 18 2026' -A x64 -DSIMNODUS_BUILD_DESKTOP=ON '-DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64'
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' $cmake --build build/sn023-resistance-value/native-build --config Release --target simnodus_document_editor editor_document_tests editor_name_history_tests editor_resistance_tests native_revision_probe
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir build/sn023-resistance-value/native-build -C Release --output-on-failure --verbose -R '^(editor-document-contracts|editor-name-history-contracts|editor-resistance-contracts|native-project-revision|native-instance-label-revision|native-resistance-revision)$'
python tests/resources/editor_resistance_acceptance.py --editor build/sn023-resistance-value/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-resistance-value/new-gui-run
python tools/check_repository.py
git diff --check
```

All six directly affected CTests passed across the retained attempts: the five
document/history/name revision tests passed on the first run; the new resistance
test passed its final targeted rerun (seven tests, 53 requests). Existing pure
project/instance-name suites retain their 33/46 requests. The new application
lifecycle passed 23 assertions and six independent persisted byte audits,
including mixed history in both directions, stale-redo replacement, escaped
no-op spelling, equal-magnitude byte edits and exact reopened state.

The first native GUI path passed all 29 controls and two persisted byte audits:
fixed-unit/target-bound display, invalid quantities/ranges, all three drafts,
other-field Apply retention, selection Cancel/Discard, Open/exit Cancel, mixed
history, explicit copy and clean reopen. The final capture was inspected: the
selected `main/left` shows `3.5 kohm` and `100` to `10000 ohm`; panels remain
separate. An independent analyzer window remains available without signal data.
The headless lifecycle also passed all 23 assertions and six byte audits with
`QT_*` removed and PATH restricted to System32; PE imports contain no Qt.

### Retained failed attempts

Configure/build and the first GUI/headless runs passed. The first CTest run failed
only the new Python oracle: topology 0.3 was passed directly to the existing 0.2
parameter resolver instead of `bindings.projection`. Read-only review also caught
a root-prefixed occurrence path and a misplaced model-interface path before the
next attempt. The second run passed six of seven tests but expected `bytes` for
an oversized original; the unchanged loader correctly reports `input` at the
base stage. Correcting that expectation produced the final seven-test pass.
Production sources/binaries remained unchanged throughout these oracle fixes.
Both failed logs and exact test-source versions are retained; their results do
not count as passes. Standard optional pthread/Vulkan discovery diagnostics are
also retained. No backend or previous GUI/layout matrix was rerun.

The [summary manifest](evidence/SN-023-resistance-value-summary.json) records
sources, binaries, commands, raw attempts, capture and preservation hashes; the
[native control report](evidence/SN-023-resistance-value-gui-01.json) records each
GUI result. Ignored raw output remains under `build/sn023-resistance-value/`.
PR #73 was reconfirmed MERGED at `4126f74898fc12b256bba1ff6053dbd70baf37ff`,
matching local/origin/remote main, with both required Foundation checks passing
(Windows 59/59; Ubuntu 48/48) and zero unpublished commits. This branch started
there. All 210 historical evidence hashes and twelve independent overlays were
preserved. Own planning edits reverse exactly to backed-up bytes; publication
stages them separately. The source PR records final required checks and squash.
No new ADR is needed for this bounded command reusing schema and operations.

## Pending beyond this increment

General parameter editing, unit/forwarded-binding changes, multi-step undo,
placement/wiring, managed Save/service, real-engine worker/IPC, instruments,
human file dialogs/recovery, keyboard/accessibility, monitor/DPI, persistent layout
and packaging remain pending. Opening stays inert. Preserve the existing SN-021
Windows/local-NTFS/one-managed-document/fixed-E-01 limits and negatives, SN-022
synthetic worker selection and failed runtime/setup attempts, and SN-017
Python/GDB/fixture ownership, accepted profiles/tolerances, MCU/toolchain
independence and PDF/PID fixes. January stabilization and February 2027 classroom
use remain planning targets.
