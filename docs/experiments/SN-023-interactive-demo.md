# SN-023: executable interactive fixture demo

Date: 2026-09-30. Status: implemented development helper; preparation/refusal
checks and one unchanged native scripted path tested. Visible human interaction
and ordinary window closure are pending. Full SN-023 remains in_progress.

## Acceptance established before implementation

The owner requested a runnable demonstration of the existing bounded editor.
Reduce this increment to an operator helper over the unchanged executable:
prepare fresh copies of the owned declaration and single SVG in separate roots,
print explicit Open/Preview/edit/Save Copy instructions, and launch a blank
interactive window with an explicitly supplied installed Qt kit. Existing
native project operations remain authoritative. No resource is automatically
opened by the editor, and no schema, geometry, edit or simulation operation is
introduced. The helper intentionally copies only the two owned input files.

Independently check exact bytes/inventory, source preservation, refusal to reuse
an output directory, prerequisite failures before output writes and retained
launch failure diagnostics. Run one existing native pin Preview acceptance path
on the audited prepared inputs and inspect the capture; reuse prior backend,
headless, worker, edit and layout evidence. Record process start separately from
visible-window/human acceptance. Preserve 226 prior evidence files and twelve
independent overlays, including all negative and inconclusive attempts. Run the
repository/staged-tree checker and required exact-source Foundation checks before
authorized squash integration. The local pre-implementation gate and raw logs
are identified by the [summary](evidence/SN-023-interactive-demo-summary.json).

The earlier occurrence placement gate stays pending: current native operations
provide no persisted geometry/placement field. A later smallest occurrence view
must establish its own acceptance and reduce to read-only as needed. This helper
changes no architecture decision and requires no ADR.

## Implemented behavior and execution

`tools/run_editor_demo.py` creates only a new output directory, `document/original.json`,
the independently rooted `explicit-resource-root/tests/schema/fixtures/assets/passive.svg`,
and English instructions. `--prepare-only` needs Python 3.10+ but no Qt/editor.
Interactive launch requires Windows, a locally built editor and an existing
compatible Qt kit with Core/Gui/Widgets and the Windows platform plugin. It checks
these before writing the demo directory, changes only the child's Qt environment,
launches the editor without arguments, and waits until it exits. Process start,
exit or launch errors are recorded in `launch.json`; stdout/stderr have separate
logs. The process record explicitly leaves window visibility unverified. The
helper downloads nothing and copies no Qt binary, executable, model or LICENSE.

Run from the repository root with an existing compatible build, adjusting paths:

```text
python tools/run_editor_demo.py --editor build/sn023-pin-preview/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/editor-demo-01
```

The build path and Qt version above identify the validated local environment;
they are not a runtime discovery or packaging guarantee. Follow the existing
[desktop build instructions](../../apps/desktop/README.md) for another machine.
Choose a new `--out` for each demonstration; an existing directory is refused.

1. **File -> Open** the printed `document/original.json`. Opening remains inert.
2. Select `resistor` under Components, then **Preview Fixture Artwork...** and
   select the printed separate resource root. This explicitly captures only the
   owned artwork; the private original `p/a`, `n/b` convention remains bounded.
3. Select `Instance: right` under `Circuit: main`. Set its existing literal
   resistance from `2.2` to `3.5` kohm and **Apply Resistance Value**. The Inspector
   shows the applied `3500 ohm`; pending text is not an applied value.
4. **Edit -> Undo Last Edit (one step)** / **Redo Last Edit (one step)** restores
   the single transition. Apply or restore all pending text first.
5. **File -> Save Copy** with a new leaf such as `edited.json`. Explicitly Open
   the copy to inspect it; Save Copy retains the original association/dirty state.
6. **View -> Open Signal Analyzer** opens the separate, currently empty window.

Components/Preview, instance Properties, adjustable panels and independent windows
remain as before. No simulation runs on any thread in this helper. Domain,
application, adapters and Qt presentation remain unchanged and separated.

## Reconfirmed baseline and measured evidence

PR #78 was reconfirmed MERGED with source `e9f5915c9125558fe034739c1d2bf822c52ce7e9`,
squash `b85b2a29b88dc0b2db438687599d7482582264ba`, successful Foundation Windows
66/66 and Ubuntu 55/55, equal local/origin/remote main and zero unpublished commits.
The branch is `codex/sn-023-interactive-demo`. All 226 prior evidence files and
twelve overlays were backed up before editing; hashes and shared-prefix/row
reversal are verified before publication. Source PR records final required checks
and squash integration; no binary/release is published.

- [Preparation evidence](evidence/SN-023-interactive-demo-preparation.json) passed
  eight independent checks: no-Qt preparation; exact bytes/separate roots/only
  intended files; existing-directory refusal without changes; missing options,
  executable and Qt prerequisites before output writes; retained invalid-executable
  launch failure; and unchanged owned source fixtures.
- One unchanged native pin Preview acceptance path, fed the audited prepared
  input bytes, passed 20 controls and one independent saved-byte audit. The final
  captured image was inspected: declared original pins, full caption, selected
  `main/right` and applied `3500 ohm`. Its disposable acceptance directory remains
  separate from the untouched prepared demo directory.
- The normal helper started the expected existing editor process and waited for
  it. Desktop discovery found no matching visible window. Only that test child
  was deliberately stopped after observation; its resulting nonzero exit/logs
  are preserved. This is process-start evidence, not normal close, human usability,
  recovery or proof that the window appeared on the owner's desktop. The earlier
  independent direct launch also remains inconclusive and retains its logs.
- No application/C++ change or rebuild was necessary. Existing native/headless
  and engine matrices are reused; no backend, worker or simulation run occurred.

Human file/directory dialogs, keyboard/accessibility, monitor/DPI, cross-session
layout, ordinary close and packaging remain pending. The helper is development
preparation, not a production launcher, Qt redistribution or a recovery flow.
No wiring/placement/general components, instruments, production Save or worker/IPC
integration is added. Keep SN-021 Windows/local-NTFS/one-managed-document/fixed-E-01
limits, SN-022 synthetic selection and failed attempts, and SN-017 preparation/GDB,
profiles/tolerances, MCU/toolchain independence and PDF/PID behavior. January
stabilization and February 2027 classroom use remain planning targets.
