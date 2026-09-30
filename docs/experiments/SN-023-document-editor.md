# SN-023: first bounded Windows document editor slice

Date: 2026-09-30. Status: **first bounded slice implemented and tested on native
Windows; full SN-023 remains in_progress**. The criteria below were written
before implementation and then checked with the recorded commands.

## Scope and acceptance

Use the existing `two-rc-project.json` declaration fixture, copied into a fresh
local NTFS directory without its referenced resources. The first editor path is
explicit inert Open, structural inspection, one top-level display-name edit,
and explicit **Save Copy** to a new filename in the original directory. Keeping
the directory fixed retains the selected acquisition-directory spelling only;
it does not verify the fixture's declared resources, establish their intended
root, or lease the physical directory identity across operations. Opening the
copy is a separate explicit operation; Save Copy leaves the original document
and its dirty state intact. It does not implement managed Save or import.

Before implementation, require these observations:

- A Qt-free application document owns the immutable native graph, original
  acquisition-directory spelling/context and current source bytes. Qt owns view
  state only.
- Native acquisition validates one selected document; opening succeeds with
  referenced resource files absent. No dependency, model, firmware or engine
  is loaded, fetched, interpreted or executed.
- Inspect project identity, component definitions/pins, circuit instances and
  explicit nets using stable source IDs. Catalog selection/Preview is separate
  from selected-instance Properties. The structural view claims no schematic
  coordinates, rendered symbols, inferred wiring or simulation readiness.
- Apply a valid Unicode/escaped display name through `rename_project`;
  independently verify only the original top-level name token changed. Invalid
  names leave the prior graph and dirty state intact. A failed Open also retains
  the current document; interactive replacement/exit requires discard approval.
- Save Copy uses `save_new_project` explicitly. Exact bytes reopen, while the
  original stays unchanged. Existing filename, original filename, invalid leaf
  name and missing destination refuse without changing existing bytes or losing
  the edit. No external overwrite or automatic Save is available.
- Independent editor/analyzer windows, adjustable Components/Preview and
  Properties docks, and panel hide/reopen work. Analyzer has no data until
  shared instrumentation exists; closing/reopening it retains the document.
- Native Windows scripted checks invoke the same application commands as the
  interactive controls, plus real selection/button signals, and report exact
  observations. Discard cancellation is scripted; file-dialog acceptance and
  human interaction remain separate pending work.
  Headless application tests require
  no Qt. Run targeted native regressions and the repository checker.

Use only optional Qt Core/Gui/Widgets presentation with the installed shared
kit inventoried by [SN-022](SN-022-qt-boundary.md). No additional package or asset
is incorporated. Reuse unchanged engine and worker evidence without claiming a
new backend run. No solver runs on any GUI thread in this slice.

## Measured result

The [evidence summary](evidence/SN-023-document-editor-summary.json) records
source, binary, log and report hashes and links the retained native reports.
Release x64 used C++20, MSVC 19.51.36246.0, Windows SDK 10.0.26100.0 and the
existing Qt 6.11.1 shared kit with native `windows` and Fusion. The host reported
Windows build 26300, independently of SN-022's earlier build 26200 observation.

Both the byte-identical declaration and a derived fixture with an HTML-like
instance name passed **23 native GUI checks each**. A real Apply Name button
and selection signals exercised separate catalog Preview and instance Properties;
all untrusted dynamic labels use plain text. The original and occupied copy
refusals, invalid Open/name/leaf, pending unapplied name and scripted discard
Cancel retained the edit. Dock/splitter resizing changed measured sizes and an
in-memory round trip restored them; panel and analyzer reopening retained the
document. This tests selected controls, not keyboard/mouse accessibility or
human discard/recovery decisions. File dialogs and discard approval are pending.

Independent Python audits compared the complete JSON values and exact byte
prefix/suffix outside the original top-level name token. Copies reopened with
exact bytes, and source documents stayed unchanged. The Qt-free native lifecycle
also tested empty state, no-op rename, Unicode/escaping, invalid names, failed
Open, occupied copy, missing directory, source disappearance and original-name
case aliases. The same lifecycle ran with all `QT_*` variables removed and PATH
restricted to System32; its direct PE imports contained no Qt.

Five targeted CTests passed: new document contracts, native revision, acquisition,
save contracts and save filesystem. The unchanged acquisition suite retained two
skips (Linux-only rejection and unavailable Windows symlink privilege); save
retained one symlink-privilege skip. These skips are not passing physical cases.
The earlier 20-check GUI runs are retained as positive narrower observations,
superseded by the final 23-check runs rather than relabeled. No runtime check
failed in this SN-023 block. Initial sandbox-denied setup operations are recorded
separately from successful builds/runs. SN-022 failures remain unchanged.

The repository checker and whitespace checks passed. All 200 preexisting evidence
files and twelve independent overlays were verified unchanged; shared documents
publish only this task's edits. PR #70's squash/main/check state was reconfirmed
before starting `codex/sn-023-document-editor`. The source PR records final
required Foundation checks and squash; no binary release is published.

## Limits and next work

This is a document/graph inspector with name editing, not completion of the
SN-023 component/wiring/undo editor. Managed saving still needs production
provisioning and an application boundary; the accepted private SN-021 endpoint
must not be promoted into a desktop service. General circuit edits, symbols,
placement, wiring, undo, instruments, simulation actions, worker integration,
keyboard/accessibility, monitor/DPI, cross-session layout and packaging remain
pending. Scripted checks do not establish human usability or recovery flows.

The [SN-021 final audit](SN-021-final-acceptance.md), ADRs 0076-0078, SN-017
Python/GDB/fixture ownership, 10 microvolt/1 ps limits, MCU/toolchain independence,
PDF suppression/PID retry and SN-044 shared instrumentation ownership remain
unchanged. Preserve January stabilization and the February 2027 classroom target
as planning constraints. This reuses decisions and adds no significant new ADR.
