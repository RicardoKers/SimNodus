# SN-023: declared instance display-name editing

Date: 2026-09-30. Status: this bounded increment is implemented and tested on
native Windows; full SN-023 remains in_progress. Acceptance below was specified
before implementation. Publication identities/checks are recorded by the source PR.

## Smallest coherent scope and acceptance

Extend the [first document editor slice](SN-023-document-editor.md) with one
editable field in Instance Properties: the declared instance **display name**.
Select the source record by circuit-definition ID and instance ID, never by
label, row or array position. The existing `two-rc-project.json` is the fixture.
This is a source-definition edit: changing a nested reusable circuit's declared
instance label affects that definition in all occurrences. It creates no
occurrence-specific override, electrical edit or simulation permission.

Before implementation, require:

- A pure Qt-free native revision changes exactly one selected `name` value
  token, preserving every other byte. Fully validate both the original and
  candidate, rebuild provenance, and retain all stable graph/source identities,
  definitions, terminals, parameters, resource declarations and temporal policy.
- Selectors remain correct with reordered arrays/keys, escaped JSON keys,
  Unicode/escaped labels, duplicate display labels and the same instance ID in
  distinct circuit definitions. Missing/malformed selectors and invalid names
  return structured errors without publishing a partial revision. Exercise the
  existing 80-scalar name and 1 MiB document bounds and repeated/no-op revisions.
- The application document composes that command with inert Open and existing
  explicit Save Copy. Invalid edits retain the exact previous graph and dirty
  state. A copy reopens with exact revised bytes; the external original stays
  unchanged. Retaining the acquisition-directory spelling establishes no
  verified resource context or physical directory lease.
- Native Windows Instance Properties selects the declared source record and
  exposes the one field/Apply operation. Catalog Preview remains independent.
  Successful edits retain the selected IDs and update the visible label. A
  net, circuit row or no selection offers no instance edit.
- Pending field text must not silently disappear on selection/Open/exit or be
  silently omitted by Save Copy. Applying or explicitly discarding text governs
  replacement; cancellation retains the prior selection and pending text.
  Scripted control checks do not establish human recovery or accessibility.
- Reuse prior window/layout and backend evidence unchanged. Run focused native
  revision/application tests, one dedicated native Windows control path, the
  repository checker and publication checks. Keep all historical results,
  including unsuccessful attempts, and all twelve independent local overlays.

Keep Qt in presentation and reuse Core/Gui/Widgets with the existing inventory.
No new asset, package, rendering system or ADR is needed for this metadata edit.
Headless application/domain code has no Qt dependency. Both windows continue
to consume application state; the analyzer has no fabricated signal data.

## Implementation and measured evidence

The Qt-free `rename_instance` operation validates the original, resolves the
source object by circuit-definition/instance IDs in the existing provenance map,
replaces its immediate name value token, then validates and owns the complete
candidate. `rename_project` shares the unchanged token-replacement rules. The
application document publishes a graph only on success and retains its original
acquisition association. No file/resource I/O occurs in either revision command.

Instance Properties provides one field and Apply button. Selection retains IDs
across a successful edit, and catalog Preview remains independent. Circuit/net
rows and no selection disable the field. Pending instance text blocks Save Copy;
selection changes use Discard/Cancel, and Open/exit guards include pending text.
Invalid names, failed opens and cancellation retain the document and draft.
Dynamic labels remain plain text, including HTML-like input.

Measured with Qt 6.11.1 Core/Gui/Widgets, MSVC 19.51.36246.0, x64 Release/C++20,
SDK 10.0.26100.0 and the native Windows platform plugin on Windows build 26300:

- Three focused CTests passed: extended `editor-document-contracts`, existing
  `native-project-revision` (11 cases/33 native requests), and the new
  `native-instance-label-revision` (8 cases/46 native requests). Independent
  Python lexical/semantic oracles compare exact unaffected bytes, the complete
  graph and all identity/source spans. Tests include reordered/escaped keys,
  repeated/no-op revisions, contextual IDs, invalid UTF-8/selectors/names,
  literal injection text, 80 scalars and the 1 MiB boundary.
- The first dedicated native GUI path passed 20 checks plus its byte audit.
  Its source/executable snapshot and raw outputs remain retained. After adding
  the net-row refusal assertion, the final path passed **21/21** and the exact
  byte audit. No implementation behavior changed between these two runs.
  Apply-button, selection, draft retention, scripted Discard/Cancel, copy/reopen
  and analyzer reopen were observed. The original remained byte-identical;
  only the `rc/r` declared name token changed in the persisted copy. The capture
  was visually inspected for separate Preview/Properties and literal label text.
- The extended headless document lifecycle passed with all `QT_*` variables
  removed and System32-only PATH. Direct PE dependencies contain no Qt. Separate
  exact-byte audits verified both project-name and instance-name copies and the
  unchanged original. This is application evidence, not an engine-worker run.

See the [summary](evidence/SN-023-instance-label-summary.json) and retained
[first](evidence/SN-023-instance-label-gui-01.json) /
[final](evidence/SN-023-instance-label-gui-02.json) control reports for identities,
commands, hashes and raw local paths. Repository and publication-tree checkers
and `git diff --check` are required before publication; the PR records their
results and both hosted Foundation checks. Twelve independent SN-045 overlays
are excluded from the staged publication. All 205 earlier evidence files remain
byte-identical, including first-slice and SN-022 failed runtime/setup evidence.
Prior window/layout/backend matrices were reused unchanged. No new ADR is needed
for this bounded metadata command under the existing ownership decisions.

Scripted dialog responses are control evidence only. Native file dialogs,
interactive discard acceptance and human recovery/usability remain unverified.

## Limits and next work

General instance properties/parameters, placement, symbols, wiring, undo,
managed Save/service provisioning, real simulation workers/instruments,
file-dialog/human usability, keyboard/accessibility, monitor/DPI, persistent
layout and packaging remain pending. Name metadata is not a circuit-instance
identity and no occurrence override is introduced. Opening remains inert.

The SN-021 Windows/local-NTFS managed-document/fixed-E-01 acceptance, SN-022
synthetic worker limits and failures, SN-017 Python/GDB/fixture ownership,
10 microvolt/1 ps bounds, MCU/toolchain independence and PDF/PID behavior remain
unchanged. Preserve January stabilization and February 2027 as planning targets.
