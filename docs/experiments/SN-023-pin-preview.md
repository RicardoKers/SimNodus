# SN-023: declared pins on the owned fixture Preview

Date: 2026-09-30. Status: implemented and tested for the closed declared-pin
fixture convention. Initial acceptance was saved before implementation; review
refined full-map retention and geometric splitting during implementation.
Full SN-023 remains in_progress; source PR records final checks/squash integration.

## Acceptance before implementation

Use only the existing original `resistor` component, `two-pin` descriptor and
227-byte owned artwork accepted by the previous explicit Preview. The SVG has
no declared anchors. Establish an explicit **closed fixture convention**, not an
inferred SVG/schema truth: descriptor anchor `a` belongs at the left open lead
endpoint, `(0,20)`, and `b` at the right endpoint, `(100,20)`, in the recognized
100 by 40 view. Derive logical pin identity from the validated declared `pin_map`.
Original `p -> a`, `n -> b` and a valid swapped map must annotate accordingly.
The convention covers this exact artwork/descriptor only, without electrical
polarity, model compatibility, general anchor ABI or changed fixture bytes.

- A pure Qt-free application inspection returns two owned logical/descriptor-pin
  IDs and positions through the existing recognized fixture adapter convention.
  Select stable IDs, not catalog order, pin order, labels, names or model mappings.
  Retain unchanged source graph/bytes, history and all resource/runtime flags.
- Explicit native capture still reads exactly one known symbol file from an
  independently chosen root. Store owned optional pin annotations with captured
  bytes/metadata/geometry. Unsupported/null/alternate descriptor anchors remain
  unavailable; recognized artwork may still be previewed without annotations.
  Do not invent positions for unknown IDs or execute/read other resources.
- Qt Core/Gui/Widgets paints endpoint markers and plain logical/anchor IDs from
  the capture using the same aspect fit. Caption explicitly identifies the closed
  fixture convention and declared mapping; unavailable anchors are distinguished.
  Repaint/resize perform no I/O. No insertion, placement, snapping or wiring.
- Compare the complete owned declared pin map by IDs, including when annotation
  is unavailable; order-only changes remain equivalent. Equivalent pins/selection
  retain the capture after existing name/R/C edits and
  one-step history. Clear it if declared mapping changes. Successful Open/catalog
  changes clear it; failed Open/capture keep the prior marked artifact. All four
  drafts and independent instance/occurrence Properties remain unchanged by Preview.
- Independent Python evidence derives lead endpoints from degree-one vertices of
  the original SVG path after splitting internal segment intersections, applies
  the explicit left/a right/b convention and reads
  declared maps. Check original/swapped maps, reordered catalog/pins/maps, escaped
  keys and duplicate/untrusted labels. Compatible-but-unknown descriptor/anchors
  return unavailable; invalid/untrusted input remains refused. Releasing source
  graph and replacing copied files preserves captured annotation ownership.
- One bounded native Qt path checks annotation/caption, original and swapped
  mappings, unsupported anchors with artwork retained, draft/nonmutation and
  Properties independence, edit/history, explicit Save Copy/reopen, failures and
  wide/narrow marker alignment. Inspect images and independently audit saved bytes.
  Reuse unchanged backend/edit/worker/layout matrices; test native inspection with
  Qt absent. Human directory-dialog flow, keyboard/accessibility, monitor/DPI,
  cross-session layout and packaging remain pending.
- Preserve all 222 prior evidence files and twelve independent SN-045 overlays,
  including failed/inconclusive compiler, runtime, setup and visual attempts.
  Update CURRENT/task/handoff, run targeted/repository/staged-tree checks, then the
  authorized branch/PR/required-Foundation/squash workflow for stable results.

This adds a private closed fixture convention to the existing adapter, not a
significant general architecture/schema decision; no new ADR is needed. General
symbols, editable anchors, placement/wiring, arbitrary components, model/firmware
execution, managed Save, workers/production IPC and instrumentation remain pending.
Keep independent Circuit Editor/Signal Analyzer windows, adjustable panels,
Components/Preview versus instance Properties and shared measurement ownership.
Qt stays outside headless domain/application/engine code; no simulation is run.

SN-021 remains Windows/local NTFS, one managed document and explicit fixed E-01;
no external overwrite, automatic executable access, production save service,
power-loss, general MCU/mixed-signal, trusted origin or redistribution proof.
SN-022 remains synthetic nine process/nine layout/Qt-free worker selection; keep
failed setup/runtime attempts and do not infer human recovery from retries.
SN-017 Python preparation/GDB/fixture control, accepted profiles/tolerances,
MCU/toolchain independence, PDF suppression and PID retries remain unchanged.
January stabilization and February 2027 classroom use remain planning targets.

## Implemented behavior

`fixture_anchor` in the Qt-free symbols adapter explicitly assigns `two-pin/a`
and `two-pin/b` to the two open lead endpoints of the previously recognized exact
artwork. No markup is parsed or changed. Pure application `inspect_fixture_pins`
first uses validated stable component/symbol/resource selection, then reads the
complete owned declared pin map. It returns two owned rows only for exact a/b
coverage; unsupported interfaces return an `anchors` refusal. Existing exact
size/hash/byte recognition remains authoritative for physical capture.

Selection owns the full map sorted by logical ID; capture owns that map, optional
annotations and immutable artwork/resource data. `retainPreview` compares both
selection/full map and optional annotations after graph refresh. Reordered maps
remain equivalent; changed maps clear the old artifact even when both lack known
anchor positions. No offset or display label participates in identity. A compatible
unknown descriptor can still capture the recognized artwork with no annotations;
this never promotes resource/interface/runtime verification flags.

QPainter draws blue endpoint dots and plain logical/anchor labels with the existing
fitted transform. Full IDs remain in a plain-text caption; drawn labels use width
elision. Wrapped metadata/caption use minimum vertical policies and a constrained
Preview layout. Components/Preview and instance/occurrence Properties remain
independent; no new edit, placement, net, simulation or save command is added.

## Reconfirmed baseline and measured results

PR #77 was reconfirmed MERGED, source `6c852f726431ed6d3f18c507e95bba9476deb770`,
squash `d84db84f86bba557eab832b56b46df4a3ad051b1`, with Foundation Windows 66/66
and Ubuntu 55/55 successful. Local main, origin/main and remote main matched and
there were zero unpublished commits. Branch `codex/sn-023-pin-preview` starts there.
All 222 prior evidence files and twelve independent overlays were backed up before
implementation. Original project/artwork bytes and prior reports are unchanged.

- Two targeted Release CTests passed first run. The expanded native Preview suite
  passed ten tests/36 independent requests, including 16 application lifecycle
  assertions and one saved-byte audit; unchanged document contracts passed too.
- Python walks the existing SVG path, splits touched edges at the lead/body joins,
  derives the two degree-one external endpoints, then applies the explicit a/b
  convention and declared map independently of C++ constants. Original/swapped,
  reordered catalogs/pins/maps, escaped keys and duplicate/untrusted labels passed.
- Unknown descriptor/anchor IDs, valid three-pin interfaces, null bindings, malformed
  input and non-bijective declarations were refused or remained unavailable as
  appropriate. Physical captures with unavailable annotations retain the full
  sorted two/three-entry map and exactly one resource. No model/LICENSE file is
  needed. Pure/capture probes release their loaded graph before serializing owned
  rows; the native lifecycle retains captured data through copied-file replacement.
- Headless execution passed 16 assertions/one byte audit with QT_* removed and
  PATH reduced to Windows System32. Direct PE imports contain no Qt. Builds 01-03
  passed using the unchanged Qt Core/Gui/Widgets module selection.
- [GUI 01](evidence/SN-023-pin-preview-gui-01.json) passed 20 controls/one independent
  persisted-byte audit, but visual inspection found clipped caption text and a
  stale swapped-map layout capture. Sources, executables, reports and images were
  preserved before setting Preview layout/label minimum policies.
- [GUI 02](evidence/SN-023-pin-preview-gui-02.json) passed 18/20 after adding wrapped
  height assertions: original/swapped caption checks observed intermediate dock
  width/height relayout. The complete final caption was visible, but this run is
  retained as failed, including its sources/binary/report and absent swapped image.
- [GUI 03](evidence/SN-023-pin-preview-gui-03.json) passed 20/20 and the independent
  saved-byte audit after settling four test-only nested layout-event passes before
  capture. Final original/swapped window and wide/narrow marker images inspected:
  full captions, p/a-n/b versus n/a-p/b and blue dots at fitted endpoints. Test
  settling is not a GUI responsiveness/DPI or human recovery guarantee.
- The Qt path retains four drafts, document/history/association and independent
  Properties through Preview; it applies only the existing right resistance edit
  (`2.2` to `3.5 kohm`), saves an exact new copy and reopens inertly. Failed operations
  preserve captured pins. Two test-only application reloads/reconsultations prove
  equivalent unavailable maps retain and changed unavailable maps clear; they are
  not a binding editor or a user recovery workflow. Analyzer reopen is independent.

[Summary/hashes](evidence/SN-023-pin-preview-summary.json) identify final source/
binary snapshots, original fixture/artwork and all retained runtime/local artifacts.
Prior evidence and ten untouched overlays match saved hashes; removing only this
task's CURRENT/BACKLOG prefix/row restores the other two exact saved overlays.
Repository/staged-tree checks and exact-head required Foundation/squash records
are reported by the source PR. No unchanged backend/edit/worker/layout matrix was
rerun; earlier failed/inconclusive results remain distinct.

## Reproduction and pending behavior

Measured configuration: Windows 10.0.26300, MSVC 19.51.36246/SDK 10.0.26100,
Release x64, Visual Studio 18 2026 generator, installed Qt 6.11.1 msvc2022_64,
native windows platform/Fusion style. Python runner 3.11.9; CTest Python 3.14.4.
Use fresh paths and the desktop README's Python environment normalization:

```text
cmake -S . -B build/sn023-pin-preview/native-build -G "Visual Studio 18 2026" -A x64 -DSIMNODUS_BUILD_DESKTOP=ON -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64
cmake --build build/sn023-pin-preview/native-build --config Release --target simnodus_document_editor native_preview_probe editor_document_tests
ctest --test-dir build/sn023-pin-preview/native-build -C Release --output-on-failure --verbose -R "^(native-fixture-preview|editor-document-contracts)$"
python tests/resources/editor_pin_preview_acceptance.py --editor build/sn023-pin-preview/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-pin-preview/new-gui-run
python tools/check_repository.py
git diff --check
```

Linux exercises pure recognition/inspection and reports physical capture/lifecycle
as unsupported Windows/local-NTFS work. Next define one bounded existing-occurrence
placement gate, including stable occurrence identity and persistence acceptance,
before implementing it; reduce to read-only placement if existing operations cannot
support a coherent edit. General anchor/schema policy, canvas/wiring, other
components, managed Save, shared instruments, workers/IPC, human file-dialog flow,
keyboard/accessibility, monitor/DPI, cross-session layout and packaging stay pending.
This convention neither validates electrical polarity nor expands SN-021/022/017
authority or numerical acceptance. Synchronous I/O latency remains unmeasured.
