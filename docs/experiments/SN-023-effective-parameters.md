# SN-023: read-only effective R/C inspection

Date: 2026-09-30. Status: implemented and tested for this bounded read-only
declaration inspection. Full SN-023 remains in_progress.

## Bounded acceptance before implementation

Inspect existing resolved declaration parameters for **one selected occurrence**
in the owned `two-rc-project.json`. The first path is `main/left`: resistance
`1000 ohm` from a literal and capacitance `0.000001 F` from the target default.
Selecting the reused `rc/r` source definition offers `main/left/r` and
`main/right/r`, with different resolved resistance and `containing-circuit`
forwarding origins. These are normalized declaration values, not measurements.

- Add a small Qt-free read-only lookup over the **already validated owned**
  ParameterSnapshot. Select the source by stable circuit-definition/instance IDs,
  map its source provenance to occurrences and return borrowed immutable rows.
  Keep the graph retained while using rows; copy full paths into presentation.
  No new resolver, snapshot cache, mutation command, domain type or ADR.
- Keep source definition and root-prefixed occurrence paths distinct. Reused
  definitions must offer explicit occurrence selection; preserve the selected
  full ID path across effective edits/Undo/Redo when it still exists. Labels or
  array indexes do not establish identity. Missing/unreachable selections yield
  no rows. Sort full paths for deterministic presentation.
- Show the existing `resistance`/`capacitance` IDs in base ohm/F, values and immediate binding origin
  (`literal`, `default`, `containing-circuit:<parameter>`). Show source offsets in
  native evidence. Forwarding origin identifies the local binding, not a full
  upstream chain. No insertion, default/forward conversion, unit edit or runtime
  authority is introduced. The four existing drafts remain separate.
- The Properties inspector is read only. No selection/circuit/net or unused
  definition has no occurrence data. Display explicit path and definition;
  render untrusted metadata as plain text. Components/Preview remains independent.
  Scroll the Properties content so the existing controls remain reachable when
  the adjustable panel is short. This is a scripted reachability check only.
- Occurrence-only selection must preserve all four drafts, graph identity/bytes,
  dirty state, history, source association and Preview. Applying a name/R/C or
  restoring history must refresh from current immutable data, preserving the
  occurrence path. Draft text must not affect displayed applied values. Failed
  edits/Open and Save Copy must not silently replace occurrence selection.
- Independent native tests compare all matched paths/definitions/parameters and
  their source offsets with Python validation/resolution over reordered/escaped
  keys, duplicate labels/IDs in different circuits, default/forward/literal and
  unreachable definitions. Verify selector nonmutation and graph/view lifetime.
- A headless application lifecycle checks inspection after R/C edit, exact
  Undo/Redo, error/no-op and copy/reopen with independent persisted-byte audits.
  One native Qt control path checks default/forwarded values, reused occurrences,
  four drafts, refresh/history/selection and explicit persistence. Inspect the
  capture and Qt-free PE imports. Reuse unchanged edit/backend/layout matrices.
  In the original fixture, actual occurrence switching at `rc/r` preserves two
  active name drafts; its forwarded R/C fields are disabled. Separately, refreshing
  the single `main/right` occurrence preserves four active drafts. Do not claim
  that this fixture proves switching occurrences with four editable fields.
- Preserve all 214 earlier evidence files and twelve independent overlays,
  including negative/inconclusive and failed attempts. Run repository/staged-tree
  checks and the branch/PR/required Foundation/squash workflow.

Qt stays Core/Gui/Widgets presentation only. Retain separate editor/analyzer
windows, adjustable panels and shared instrumentation ownership. Opening stays
inert; this slice loads no resource and offers no engine execution.

## Implemented and measured

The small [native lookup](../../src/application/project_inspection.hpp) selects
by source circuit-definition/instance IDs and matches existing owned source
provenance to the already resolved occurrence rows. It performs no I/O or new
resolution. Borrowed rows require a retained graph. Presentation stores its own
full path IDs, reconsults the current immutable graph after edits/Undo/Redo and
blocks signals while rebuilding the occurrence selector. The read-only table
shows the existing resistance/capacitance IDs, base units and immediate origins;
unreachable source instances return no occurrence. No domain/engine code changed
and no significant architecture decision or ADR was needed.

Three focused Release CTests passed on the first run: the independent six-test
inspection suite made **29 requests**; the application lifecycle passed **17
assertions and three persisted-byte audits**; the existing document contracts
passed unchanged. Tests cover defaults, forwarding, changed upstream values,
stable paths, duplicate labels/IDs across definitions, reordered/escaped keys,
non-ASCII label offsets, unused definitions, missing selectors, untrusted input,
borrowed-view lifetime, nonmutation, current/old graphs, failures and one-step
history. The native lifecycle also passed with all Qt environment/kit PATH entries
removed; direct PE dependencies contain no Qt. Ubuntu's required check can test
pure lookup and retained graph views, but skips the Windows physical lifecycle.

The first Qt path passed 32/33 controls. One assertion incorrectly expected a
particular Preview placeholder after explicit Open, although Open refreshes that
independent panel. Its report, screenshot, runner, sources and executable are
retained. The corrected assertion compares Preview before/after subsequent
inspection; no production behavior changed. The second path passed **33/33
controls and two independent saved-byte audits**. Actual reused-path switching
preserved two active name drafts with forwarded R/C disabled. Refreshing the
single `main/right` path preserved all four active drafts and displayed applied
values until each Apply. Quantity/name changes and history refreshed values while
retaining full occurrence paths; explicit Save Copy/Open and failures behaved
within the existing authority. A short panel allowed scripted scrolling to the
capacitance button and back to inspection. The final capture was inspected.

Run commands, versions, hashes and limitations are in the
[summary](evidence/SN-023-effective-parameters-summary.json). Both GUI reports
are retained: [first failed oracle](evidence/SN-023-effective-parameters-gui-01.json)
and [final pass](evidence/SN-023-effective-parameters-gui-02.json). Raw local
attempts live under ignored `build/sn023-effective-parameters/`; source/binary
hashes bind them to this report. A local helper-edit quoting/dispatch failure was
also recorded; it did not execute a build or test. Optional pthread/Vulkan
discovery diagnostics remain in the successful configure log.

PR #75 was reconfirmed MERGED: source `f4923116e41a863e669b39ab43362e835f55ebe3`,
squash `661de58b380905e7ad40576965b5e6f5cb97a3e5`, with Foundation Windows
63/63 and Ubuntu 52/52 successful. Local/origin/remote main matched and no
unpublished commit existed before branch `codex/sn-023-effective-parameters`.
All **214 historical evidence files and twelve independent SN-045 overlays**
are preserved; only this task's portions of CURRENT/BACKLOG are published.
Repository/staged-tree checks and final required-check/squash identities are
recorded by the source PR. Unchanged backend/edit/layout matrices were reused.

## Reproduce the focused path

Use the [existing Qt build instructions](../../apps/desktop/README.md), then build
`native_inspection_probe`, `editor_inspection_tests`, `editor_document_tests` and
`simnodus_document_editor`. Run these existing CTest names:

```text
ctest --test-dir build/sn023-effective-parameters/native-build -C Release --output-on-failure --verbose -R "^(native-parameter-inspection|editor-inspection-contracts|editor-document-contracts)$"
python tests/resources/editor_inspection_acceptance.py --editor build/sn023-effective-parameters/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/sn023-effective-parameters/new-gui-run
python tools/check_repository.py
```

## Pending beyond this increment

Full provenance chains, general parameter editing, unit/binding conversion,
multi-step history, placement/wiring, managed Save/service, engine workers/IPC,
instruments, human recovery, keyboard/accessibility, monitor/DPI, cross-session
layout and packaging remain pending. Preserve SN-021 Windows/local-NTFS/
one-managed-document/fixed-E-01 limits and negatives; SN-022 synthetic selection
and failed setup/runtime attempts; SN-017 Python/GDB/fixture control, profiles/
tolerances, MCU/toolchain independence and PDF/PID behavior. January stabilization
and February 2027 classroom use remain planning targets.

Next specify one owned component-symbol Preview gate with acceptance first. The
unchanged RC fixture has symbol interfaces/pin mappings, but no renderable symbol
geometry. A Preview experiment therefore needs explicit owned geometry evidence;
it must not infer appearance or access external executable/native resources.
