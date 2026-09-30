# SN-023: existing literal capacitance value

Date: 2026-09-30. Status: implemented and tested for the closed capacitance
value command and four-draft mixed history. Full SN-023 remains in_progress.

## Bounded acceptance before implementation

Extend the [resistance slice](SN-023-resistance-value.md) with one existing literal
`capacitance.value` field. The first path is `main/right` in the owned
`two-rc-project.json`: `220 nF`, inclusive target bounds `0.000000001` through
`0.001 F` (1 through 1000000 nF). This edits the selected source declaration and
its occurrences; it grants no simulation or resource access.

- Keep public native commands closed to resistance and capacitance. Reuse a
  private lookup/revision helper for these two implemented fields, with one
  borrowed view shape. Add no arbitrary parameter command, framework or new ADR.
- The capacitance command supports only existing literal F/uF/nF bindings with
  fixed declared unit and target base F. No insertion, default/forward conversion
  or unit edit. Select by circuit-definition/instance IDs and decoded immediate
  keys. Fully validate the original and candidate with the unchanged loader.
- Replace exactly the selected value token; preserve all other bytes and rebuild
  complete graph/provenance/effective parameters. Independent tests verify only
  the right capacitance/forwarded capacitor descendant changes; left subtree and
  resistance remain unchanged. Include narrowed target bounds, all three units,
  boundaries/outside values, grammar/UTF-8, missing/forwarded/wrong dimension,
  reordered/escaped keys, escaped no-op and 1 MiB ceiling. Different valid numeric
  spelling is a real byte edit even when magnitude is equal.
- Reuse the single immutable history counterpart for names, resistance and
  capacitance. Invalid/no-op commands preserve it; effective edits replace it.
  Test mixed R/C/name transitions, exact undo/redo, dirty baseline, copy/reopen.
- Instance Properties adds a value field with fixed unit and target bounds.
  Four drafts are independently retained across other-field Apply. Save Copy
  and Undo/Redo block on any pending field. Instance name/R/C drafts guard
  selection; Cancel retains all, Discard clears all three local fields while
  retaining project text. Open/exit Cancel preserves pending capacitance.
- Use one native Windows control path, direct headless lifecycle and independent
  saved-byte audits. Reuse existing layouts, worker and backend evidence. Repeat
  only affected native revision/history tests; inspect Qt-free PE dependencies.
- Preserve all 212 earlier evidence files and twelve independent overlays,
  including failed resistance Python-oracle and SN-022 setup/runtime attempts.
  Run repository/staged-tree checks and branch/PR/Foundation/squash workflow.

Qt stays Core/Gui/Widgets presentation only; retain separate editor/analyzer
windows, Components/Preview versus Properties, adjustable panels and shared
instrumentation ownership. Opening remains inert. No engine work runs in the GUI.

## Implemented and measured

`literal_capacitance` returns borrowed strings from the retained validated graph;
`revise_capacitance` changes exactly one existing value token. It shares a private
lookup/revision helper with the closed resistance command, and both expose the
same `LiteralValueView` shape. Public commands do not accept an arbitrary
parameter ID. Complete original/candidate loading and exact-decimal/range
validation remain unchanged. `EditorDocument::set_capacitance` uses the existing
one-step immutable history; presentation copies/re-resolves views after mutation.

The first configure/build/CTest/GUI/headless attempts all passed. Release x64 used
C++20, MSVC 19.51.36246.0, Windows SDK 10.0.26100.0, installed Qt 6.11.1
Core/Gui/Widgets with native `windows` plugin and Fusion style. No downloads.
Eight directly affected CTests passed: document, name history, resistance and
capacitance lifecycle plus project-name, instance-name, resistance and capacitance
revision suites. The new six-test suite checked 64 requests with independent exact
bytes, complete graph/source spans and effective parameters. All three units,
narrowed selected rc bounds (keeping component bounds broad), valid alternate
dimension, escaped keys/values, invalid text, size ceilings and unaffected left
subtree/resistance passed. Existing 33/46/53 name/resistance requests also passed.

The new native lifecycle passed 21 assertions and seven independently audited
persisted snapshots: capacitance alone, clean undo, R/C together, new C transition,
project/instance names retaining R/C, and escaped numeric spelling. Original,
escaped source and occupied sentinel remained unchanged. The first Qt path passed
31 controls and two saved-byte audits: fixed-unit/bounds display, invalid input,
all four drafts, R/C cross-Apply retention, selection Cancel/Discard, Open/exit
Cancel, mixed one-step restore, explicit copy/refusal and clean reopen. The final
capture was inspected: selected `main/right` shows `470 nF` and `3.5 kohm` with
separate bounds/Apply controls, retained independent panels and analyzer window.
No signal data is fabricated. Headless execution also passed all 21 assertions
and seven audits with `QT_*` removed and PATH restricted to System32. Direct PE
dependencies contain no Qt. A focused read-only review found no material gap.

Measured commands from the repository root (use a fresh GUI output directory):

```powershell
$cmake = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' $cmake -S . -B build/sn023-capacitance-value/native-build -G 'Visual Studio 18 2026' -A x64 -DSIMNODUS_BUILD_DESKTOP=ON '-DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64'
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' $cmake --build build/sn023-capacitance-value/native-build --config Release --target simnodus_document_editor editor_document_tests editor_name_history_tests editor_resistance_tests editor_capacitance_tests native_revision_probe
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir build/sn023-capacitance-value/native-build -C Release --output-on-failure --verbose -R '^(editor-document-contracts|editor-name-history-contracts|editor-resistance-contracts|editor-capacitance-contracts|native-project-revision|native-instance-label-revision|native-resistance-revision|native-capacitance-revision)$'
python tests/resources/editor_capacitance_acceptance.py --editor build/sn023-capacitance-value/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-capacitance-value/new-gui-run
python tools/check_repository.py
git diff --check
```

The [summary manifest](evidence/SN-023-capacitance-value-summary.json) records
sources, binaries, commands, raw logs, capture and preservation hashes. The
[control report](evidence/SN-023-capacitance-value-gui-01.json) records each GUI
result. Ignored raw artifacts remain under `build/sn023-capacitance-value/`.
No failed local test attempt occurred in this increment; standard optional
pthread/Vulkan discovery diagnostics are retained. Earlier failed resistance
oracles, SN-022 runtime/setup attempts and all other evidence remain unchanged.
No backend or previous GUI/layout matrix was rerun.

PR #74 was reconfirmed MERGED at `0ba339f4fa2f38f2ea61ad6eb8e6c3ccbc023978`,
equal to local/origin/remote main, with both required Foundation checks successful
(Windows 61/61; Ubuntu 50/50) and zero unpublished commits before starting this
branch. All 212 prior evidence files and twelve independent overlays are preserved.
Own CURRENT/BACKLOG edits reverse exactly to saved bytes and are staged separately.
The source PR records final repository/staged-tree/check/squash results. No new
ADR is needed for this bounded field reusing existing schema and operations.

## Pending beyond this increment

General parameters, unit/binding changes, multi-step history, placement/wiring,
managed Save/service, real-engine workers/IPC, instruments, human recovery,
keyboard/accessibility, monitor/DPI, cross-session layout and packaging remain
pending. Preserve SN-021 Windows/local-NTFS/one-managed-document/fixed-E-01 limits
and negatives, SN-022 synthetic selection only, SN-017 Python/GDB/fixture ownership,
profiles/tolerances, MCU/toolchain independence and PDF/PID fixes. January
stabilization and February 2027 classroom use remain planning targets.
