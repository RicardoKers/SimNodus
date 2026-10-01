# SN-023: explicit owned fixture artwork Preview

Date: 2026-09-30. Status: implemented and tested for this closed owned fixture
artwork path. Acceptance was saved before implementation. Full SN-023 remains
in_progress; source PR records final required checks and squash integration.

## Bounded acceptance before implementation

Render **one original owned artwork** for the existing `resistor` component
definition in `two-rc-project.json`, before insertion and independently of instance
Properties. The original `passive.svg` is 227 bytes, SHA-256
`94bfd7453b5604800ebd5870586d51ec369eca1b3a59b04b2ec0890eb98579d2`.
It contains a 100 by 40 view and one path with two leads and a rectangular body.
Earlier handoffs identified the descriptors' lack of inline geometry; the separately
referenced external owned SVG contains this unvalidated artwork. Preserve its
original bytes/comment and all historical reports.

- Opening and catalog selection remain inert. Add **Preview Fixture Artwork...**
  requiring an explicit independently chosen resource root. Do not treat the
  document directory as an authorized resource context or physical lease.
- Native application selection reads the validated retained graph by stable
  component ID (`resistor` in this closed slice), resolves its explicit symbol
  descriptor -> asset -> dependency/resource mapping, and captures **only that
  one symbol request** through existing native local-resource verification.
  Do not access model, license, firmware or other locked files. Null/unsupported
  component/symbol selections report unavailable; no label/order-based fallback.
- Limit declared bytes/hash to the known artwork before physical capture. A small
  Qt-free symbol adapter recognizes **only the exact unchanged owned SVG bytes**,
  then supplies six known line segments and the fixed view bounds. Reject every
  other byte sequence, including otherwise valid SVG with its own valid hash.
  This is neither an SVG parser nor a stable geometry/anchor ABI. No new schema,
  toolkit module, resource execution or significant architecture ADR is needed.
- Qt Gui/Widgets paints the captured recognized artifact with aspect-preserving
  fitting and padding. Render untrusted labels as plain text. Show selected
  component, captured resource identity/hash and explicitly unverified pin-anchor
  correspondence. Captured-byte identity does not prove trusted origin, component
  appearance truth, electrical behavior or redistribution permission.
- Capture owns immutable bytes and geometry; keep it across unrelated name/R/C
  Apply, Undo/Redo and instance selection. Successful Open and changing catalog
  component clear the old artifact and require explicit new Preview. Failed
  capture retains the earlier artifact marked as the prior capture, with a clear
  refusal; no silent refresh from changed files. Do not reread on repaint/resize.
- Preview actions preserve graph bytes/identity, all four drafts, dirty state,
  history, source association and instance/occurrence selection. Preview is not
  insertion, placement, wiring, symbol editing or model verification.
- Establish independent pure geometry/mapping evidence and Windows physical
  capture cases: original only; altered/script/external-resource/oversized bytes
  rejected; reordered/escaped source keys and duplicate labels; null mapping;
  missing/wrong-size/hash symbol; selected-only capture succeeds with all other
  lock files absent. Retained bytes survive source-file replacement. Linux tests
  pure parts and reports the native physical platform limit explicitly.
- One scripted native Qt path checks Preview, independent Properties, drafts,
  selection/history, failed recapture, explicit persistence and copy/reopen, plus
  wide/narrow image fitting. Inspect captured images and use independent saved-byte
  audits. Dialog/human usability, arbitrary symbols/anchors, DPI/monitor behavior,
  cross-session layout and packaging remain pending. Reuse unchanged backend/edit/
  worker/layout matrices; keep Qt out of headless application/adapter execution.
- Preserve all 217 earlier evidence files and twelve independent SN-045 overlays,
  including every failed/inconclusive attempt. Update CURRENT/task/handoff, run
  targeted/repository/staged-tree checks, and use PR/required-check/squash workflow.

Retain SN-044's independent Circuit Editor/Signal Analyzer windows and shared
instrumentation ownership, adjustable panels and Components/Preview versus
instance Properties. No measurement is generated and no simulation runs on the
GUI thread. SN-017's Python/GDB/fixture control, profiles/tolerances, MCU/toolchain
independence and PDF/PID behavior stay unchanged. SN-021 remains limited to the
accepted Windows/local-NTFS/one-managed-document/fixed-E-01 evidence; this explicit
read-only artwork path grants no managed Save, overwrite, executable resource,
power-loss, general MCU/mixed-signal, trusted-origin or distribution authority.
SN-022 remains synthetic module/worker selection, including its failed runtime/
setup attempts; scripted retries do not establish a user recovery flow. January
stabilization and February 2027 classroom use remain planning targets.

## Implemented behavior and separation

`src/application/symbol_preview.*` reads the existing validated source map and
decoded source tokens by stable ID, copies the declared selection and invokes
native verification with a span containing one resource request. Early declared
size/hash gating excludes arbitrary assets before I/O. The unchanged graph's
global lock/interface/runtime flags remain false. The captured root is a requested
spelling, not a trusted context or retained physical directory lease.

`src/adapters/symbols/fixture_artwork.*` compares exact bytes against the original
owned SVG; six fixed segments are returned only after recognition. No XML parser,
QtSvg dependency, implicit external access, geometry ABI or pin-anchor inference
is added. `apps/desktop/preview_canvas.*` uses Qt Gui's QPainter and Widgets, with
12 logical-pixel padding and aspect-preserving fitting. Metadata is plain text.
The same artwork is referenced by other fixture descriptors: this path does not
validate component appearance or electrical correspondence.

The resource chooser starts without an inferred document root. A capture owns
metadata, bytes and geometry independently of the original graph and filesystem.
The presentation retains it across equivalent source selection after unrelated
edits/history, and clears it on catalog change or successful Open. A refusal
rebuilds the caption with the prior capture marker and native error. Captured
resource identity and full hash/root/path remain inspectable in the tooltip.

## Measured acceptance and retained attempts

PR #76 was reconfirmed MERGED at `a364ba2088e7b07c796edeb464fe64a8d25c7f2d`,
source `b656515fd1bcc8c0fe97c7d32b171c9c6d8d7a9a`, with Foundation Windows 65/65
and Ubuntu 54/54 successful. Local main, origin/main and remote main matched,
with no unpublished commits. Branch `codex/sn-023-fixture-preview` starts there.

- Two targeted Release CTests passed first run: seven independent Preview tests
  issuing 23 requests, 13 native lifecycle assertions/one persisted-byte audit,
  and unchanged document contracts. Python independently derives the six lines
  from the original SVG path, resolves the mapping and audits saved source bytes.
- Selected-only native capture succeeds with the model/LICENSE resources absent.
  Missing, wrong-size and wrong-hash files fail. Altered/script/external/oversized
  bytes, null/unsupported mapping and rehashed valid SVG fail. Reordering, escaped
  keys and duplicate labels preserve stable selection; released source graph and
  replaced physical bytes leave captured data/metadata/geometry owned and intact.
- Headless lifecycle and the byte audit passed with QT_* removed and PATH reduced
  to Windows System32. Direct PE dependencies contain no Qt. The optional GUI
  still selects only Qt Core/Gui/Widgets; no engine is invoked.
- [GUI 01](evidence/SN-023-fixture-preview-gui-01.json) passed 30 controls/two byte
  audits, but visual inspection found overlapping wrapped Properties text.
  Its production sources, executable, reports and images are retained. Explicit
  layout minimum sizing and vertical label policies fixed the overlap.
- [GUI 02](evidence/SN-023-fixture-preview-gui-02.json) passed the same 30 controls.
  [GUI 03](evidence/SN-023-fixture-preview-gui-03.json) added a 31st layout/reachability
  check and passed; its center-only reachability oracle allowed a partially
  clipped last-button image. The final oracle requires the complete button rect
  after scrolling to the final extent. All earlier attempts remain preserved.
- [GUI 04](evidence/SN-023-fixture-preview-gui-04.json) passed all 31 controls and
  two independent persisted-byte audits. Wide/narrow six-segment pixels/fitting
  passed while the copied source SVG was replaced. Final full-window, wide/narrow
  artwork and bottom Properties captures were inspected; the final button is
  completely visible. Four drafts, independent catalog/instance/occurrence state,
  history, failed operations and explicit copy/reopen retain their bounded behavior.
- The first build failed /W4 /WX because two test locals shadowed QWidget::data.
  Only their names changed. Pre-repair sources/logs are retained; the snapshot
  was taken after an oracle pixel adjustment during that build, so it is not
  claimed to be an exact reconstruction of every compiler input. Builds 02-05
  passed. Preparation usage-limit, shell creation and directory creation failures
  are recorded separately; they produced no acceptance/source mutation.

[Summary and hashes](evidence/SN-023-fixture-preview-summary.json) record final
sources/binaries, runtime reports and retained local artifacts. All 217 prior
evidence files and twelve independent SN-045 overlays are verified unchanged,
with only this task's CURRENT/BACKLOG prefix/row included in publication. Unchanged
backend/edit/worker/layout matrices were reused. Repository and staged-tree checks
and exact-head Foundation results are recorded by the source PR.

## Reproduction and next bounded gate

Measured host: Windows 10.0.26300, MSVC 19.51.36246, SDK 10.0.26100, Release x64,
Visual Studio 18 2026 generator, installed Qt 6.11.1 msvc2022_64, windows platform,
Fusion style. Python runner 3.11.9; CMake selected Python 3.14.4 for CTest.
Use fresh output/build paths to retain earlier evidence:

```text
cmake -S . -B build/sn023-fixture-preview/native-build -G "Visual Studio 18 2026" -A x64 -DSIMNODUS_BUILD_DESKTOP=ON -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64
cmake --build build/sn023-fixture-preview/native-build --config Release --target simnodus_document_editor native_preview_probe editor_document_tests
ctest --test-dir build/sn023-fixture-preview/native-build -C Release --output-on-failure --verbose -R "^(native-fixture-preview|editor-document-contracts)$"
python tests/resources/editor_preview_acceptance.py --editor build/sn023-fixture-preview/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-fixture-preview/new-gui-run
python tools/check_repository.py
git diff --check
```

Use the established Python subprocess environment normalization in the desktop
README on this host. Linux CI exercises pure recognition/mapping and explicitly
skips the unsupported Windows/local-NTFS physical lifecycle.

Next establish acceptance for one explicit owned pin-to-artwork correspondence
before adding placement or wiring. No general SVG acceptance, additional component
appearance, anchor geometry or editor canvas is implied. Human directory-dialog
recovery, keyboard/accessibility, monitor/DPI behavior, cross-session layout,
packaging, real-engine workers/production IPC and shared instruments remain pending.
Document/resource I/O is synchronous and no latency/responsiveness claim is made.
All SN-017/021/022 restrictions above remain in force.
