# SN-023: one-step name undo/redo

Date: 2026-09-30. Status: bounded one-step name history implemented and tested
on native Windows. Acceptance was specified before implementation; full SN-023
remains in_progress. The source PR records final publication checks and squash.

## Bounded acceptance before implementation

Extend the [declared instance-name slice](SN-023-instance-label.md) with one
undo/redo step for the existing applied project/instance display-name commands.
Use only the owned `two-rc-project.json` fixture. Do not add a general command
framework, schema editing, a multi-step stack or simulation operations.

- The Qt-free application document owns the current/opened immutable validated
  graphs and at most one undo OR redo counterpart. Undo/redo restore the exact
  prior owned bytes, graph and provenance; they perform no I/O or resource access.
  With the existing 1 MiB declaration cap, retained document revisions are bounded
  by at most three application-owned validated graphs (opened/current/counterpart).
  Caller-held copies are outside that bound. This is a revision-count bound, not
  a measured heap-byte or performance guarantee.
- Each effective applied project or instance-name change replaces the previous
  undo step and clears redo. Invalid edits and semantic no-ops preserve current
  graph and history. Repeated undo/redo without an available step returns a
  structured error without changing graph, dirty state or association.
- Dirty compares exact current bytes to the opened baseline. Undo of the first
  edit returns clean; undo of a later edit retains earlier edits and may stay
  dirty. Redo restores exact edited bytes. Save Copy retains association, dirty
  state and history. Failed Open/Save retains history; successful inert Open,
  including reopening a saved copy, clears it and establishes a clean baseline.
- Headless Windows lifecycle acceptance must interleave both name commands,
  failed edits, no-ops, undo/redo, a new edit after undo, Save Copy and failed /
  successful Open. Audit exported snapshots independently against exact original
  and expected name-token revisions. Preserve stable IDs and complete source maps.
  Linux tests keep explicit unsupported physical-acquisition behavior; this does
  not establish Linux desktop or filesystem support.
- Qt Edit actions explicitly advertise one step. Their availability follows
  history and both pending text fields. A command guard refuses undo/redo while
  project or selected-instance text is unapplied, retaining the draft and graph;
  users explicitly apply or restore the text. Text-field typing is not document
  history. No document shortcut is introduced in this slice.
- Restoring a name updates the title, project field and selected-instance field
  without replacing stable selection IDs or catalog Preview. The separate
  analyzer and adjustable panels retain their roles. Dedicated native Windows
  control evidence checks actual Edit action triggers, pending-text refusals,
  no-selection history, dirty/title transitions, exact copy/reopen and failed
  Open retention. Scripted controls do not establish human recovery/usability.
- Reuse earlier revision, window/layout and backend evidence unchanged. Run
  focused application/GUI checks, a Qt-free execution/import audit, the repository
  checker and the required Foundation publication workflow. Preserve all 208
  earlier evidence files and all twelve independent local SN-045 overlays.

This local bounded metadata-history policy does not change domain, application,
presentation, instrumentation or adapter ownership; no new ADR is planned.
Qt remains Core/Gui/Widgets presentation only. Opening stays inert; source
directory spelling grants no verified resource root or physical lease.

## Implementation and measured results

`EditorDocument` owns the previous or next immutable graph for one applied name
transition. Both revision commands publish only an effective byte change and
replace that counterpart; a semantic no-op returns the exact current snapshot.
Undo/redo move the already validated snapshot without parsing, file I/O, authority
changes or new graph allocation. The opened graph remains the exact dirty baseline.
Public callers can retain their own snapshots beyond the application-owned bound.

The Qt Edit menu exposes **Undo Last Name Edit (one step)** and **Redo Last Name
Edit (one step)**. Pending project or instance text disables both actions, and
direct command guards refuse it too. Successful restoration updates both names,
Properties and title using current selected IDs; it does not rebuild view state.
No document shortcut or `QUndoStack`/general command framework was introduced.

Measured with Qt 6.11.1 Core/Gui/Widgets, native Windows platform/Fusion style,
MSVC 19.51.36246.0, C++20/x64 Release and SDK 10.0.26100.0 on Windows build 26300:

- **2/2 focused CTests passed:** the unchanged document lifecycle and the new
  name-history lifecycle. The latter passed 28 native assertions and seven
  independent persisted snapshot byte audits. It interleaves both name commands,
  invalid/no-op commands, unavailable directions, repeated exact graph/source-map
  restoration, branching after undo, Save Copy/collisions, failed Open, clean
  copy Open and same-path reopening. An owned escaped-name variant proves the
  distinction between decoded name equality and exact dirty-baseline bytes.
- **26/26 native Qt controls passed**, plus two independent persisted byte audits.
  Actual Apply buttons and Edit action triggers, both direct draft guards,
  disabled/enabled states, exact first/later restoration, dirty/title updates,
  selection/Preview retention, restoring another instance while a different one
  is selected, new edit after undo, history without selection, failed Open and
  copy/reopen/reset were checked. Original bytes remained untouched. The final
  screenshot was visually inspected for the Edit menu, separate panels and
  disabled Properties after reopening without a selection.
- The same native history lifecycle and seven byte audits passed with all `QT_*`
  variables removed and System32-only PATH. Direct PE dependencies contain no Qt.
  This does not execute an engine or validate real-worker integration.

The [summary](evidence/SN-023-name-history-summary.json) and
[native control report](evidence/SN-023-name-history-gui-01.json) retain source,
binary, fixture and raw local artifact hashes, commands and scope. All 208 earlier
evidence files remain byte-identical and all twelve independent SN-045 overlays
are excluded from publication. Earlier revision, backend and window/layout
matrices were reused unchanged. The initial configure/build/test/control path
passed; the existing optional Vulkan-header/pthread configure diagnostics are
retained and did not prevent the selected Windows Widgets build. No new ADR was
needed for this local bounded history policy under existing ownership decisions.

Repository and publication-tree checkers, diff checks and both hosted Foundation
checks are required before squash; final results and identities are recorded by
the source PR. These scripted controls establish no human recovery, file-dialog
or keyboard/accessibility acceptance.

## Pending beyond this increment

Multi-step/general undo, text-field keyboard acceptance, electrical properties,
placement/wiring, managed Save/service, real-engine worker/IPC, instruments,
human file dialogs/recovery, accessibility, monitor/DPI, cross-session layout and
packaging remain pending. SN-021's Windows/local-NTFS/one-managed-document/fixed
E-01 acceptance and negatives, SN-022's synthetic worker selection and failed
runtime/setup attempts, SN-017's Python/GDB/fixture ownership, accepted profiles
and numerical tolerances, MCU/toolchain independence and PDF/PID fixes remain
unchanged. Preserve January stabilization and February 2027 as planning targets.
