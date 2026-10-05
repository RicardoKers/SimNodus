# SN-023: exact engineering-unit captions on the fixed RC canvas

Date: 2026-10-05. Status: implemented and tested for the existing bounded RC
view; SN-023 remains in_progress. General circuit editing and simulation remain
pending.

## Acceptance and implementation

The owner reported that the previous demonstration worked and authorized
continuation. This is informal positive feedback, not a structured human
usability, recovery, monitor/DPI or classroom acceptance result. The gate recorded
before implementation is hashed in the [summary](evidence/SN-023-rc-captions-summary.json).

One Qt presentation helper formats the already resolved exact decimal values for
the fixed RC canvas and read-only Circuit Properties: 2200 ohm becomes **2.2 kΩ**,
0.000000220 F becomes **220 nF**, and 0.000001 F becomes **1 µF**. Properties also
shows an explicit **Base value** line with the original resolved value and unit.
The diagram and panel query current applied values; pending text is separate.

The display table covers positive R values in [1, 1e9) ohm with Ω/kΩ/MΩ and C
values in [1e-9, 1) F with nF/µF/mF. The selected mantissa is in [1, 1000), with
exclusive upper bounds. Only canonical unsigned fixed decimal ASCII of at most
128 characters is formatted; resolved output can exceed the native input limit
of 64 characters. Decimal-point movement and removal of dispensable fractional
zeros are exact string operations, without binary floating point or rounding.
Zero, signs, other units, out-of-table magnitudes, unexpected syntax and longer
input retain the exact original value plus original unit. The English UI keeps
the decimal dot, Ω U+03A9 and micro sign U+00B5. This is a display policy, not a
quantity-entry parser, locale feature or conversion authority.

Stored/resolved strings, fixed edit units and limits, the shared C field/draft,
full IDs, geometry/hit testing, native operations, history, persistence and inert
opening remain unchanged. There is no domain/application/schema/worker/kernel
change, new dependency or Qt module. Core/Gui/Widgets stay in presentation.
Components/Preview, independent Circuit Editor and Signal Analyzer windows,
adjustable panels and shared instrumentation ownership remain independent.

## Measured evidence

Pre-edit PR #86 was reconfirmed MERGED: source
9264409380bced08555639436e7115a10474caed, squash
2812d6cf0735d7bbb5ead912280111e7999a3526. Exact-source Foundation Windows 67/67
and Ubuntu 56/56 passed in workflow 37148496021. Local main, origin/main and live
remote main matched, with zero unpublished commits. Branch codex/sn-023-rc-captions
preserves 247 earlier evidence files and twelve independent local SN-045 overlays.

- Fresh Windows C++20 Release configure and editor build passed first run. The
  final binary uses Qt 6.11.1, the native Windows platform and Fusion style.
- [GUI01](evidence/SN-023-rc-captions-gui-01.json) passed the unchanged 41-control
  C workflow first run, with fourteen independently resolved declared snapshots
  and one exact saved C-token byte audit. Input files and all other copy bytes
  remained unchanged, and its root contained no resources. It covers draft 470
  with applied 220, Apply 470, invalid/stale/default/target refusals, retained
  drafts, Undo 220, Redo 470, explicit copy/reopen and independent left C.
- The [independent caption audit](evidence/SN-023-rc-captions-caption-audit-01.json)
  passed first run: 29 exact helper cases, 26 component captions in thirteen
  supported snapshots and twelve selected Properties/base-value blocks. Its
  Python Decimal oracle uses precision 200 and numeric threshold/scaling logic
  independent of the C++ digit-shift algorithm. Cases cover prefix boundaries,
  trailing zeros, long exact precision, an 85-character resolved value and
  sign/zero/unknown-unit/syntax/magnitude/129-character fallback. Inert and
  unsupported observations have no stale Applied/Base value block.
- Active, draft, left and final images were inspected. The normal fixture's
  2.2 kΩ/220 nF, 1 kΩ/1 µF and final 470 nF captions and explicit base values
  are legible; the draft stays unapplied until Apply. Four captures are retained.
  This does not prove arbitrary long-label layout or physical mouse, human
  dialogs/recovery, keyboard/accessibility, monitor/DPI or session behavior.
- One pre-runtime patch attempt failed context verification without changing a
  file. Its diagnostic is retained before the corrected patch; it is not a Qt
  setup/runtime failure or a tested human recovery flow. No configure, build or
  runtime attempt failed in this slice. A subsequent evidence-helper attempt
  was denied at its first documentation write by sandbox file permissions;
  the diagnostic and frozen byte-identical sources/binary were retained for
  an authorized documentation retry. All earlier negatives and inconclusive
  evidence, including SN-022's first setup/runtime failures, remain preserved.
- Twenty-three sources and the final binary are frozen. Twenty unchanged prior
  source/binary entries and fourteen mixed native/source/binary artifacts match
  their reuse hashes. Canvas resolver/selection/geometry/hit-testing and native
  edit/draft/history/copy routing sections also match before/after byte hashes.
  Earlier canvas 33 controls/18 snapshots and occurrence 80 controls/11 C
  observations/two R/C byte audits are reused for unchanged behavior, not claimed
  as fresh render checks. Native 150 requests, lifecycle 14 assertions/one audit,
  no-Qt, Preview, backend, worker and layout evidence are reused without local
  matrix reruns. The source PR records the checker, exact-source required checks,
  guarded squash and final main confirmation.

No significant architecture decision is selected; no ADR is added. Task-only
publication includes only this task's planning prefix and SN-023 row, excluding
the twelve independent overlays.

## Try the local build

Use the installed development kit and a fresh output root:

```text
python tools/run_editor_demo.py --editor build/sn023-rc-captions/native-build/apps/desktop/Release/simnodus_document_editor.exe --qt-kit C:/Qt/6.11.1/msvc2022_64 --out build/rc-captions-demo-01
```

1. File -> Open the printed document/original.json. The right RC shows 2.2 kΩ
   and 220 nF. Click C to inspect its exact Base value: 0.000000220 F.
2. Click Edit Capacitance, type 470 in the fixed nF field. The diagram and applied
   Properties remain 220 nF until Apply Capacitance Value.
3. Apply: the canvas shows 470 nF and Base value becomes 0.000000470 F. Try Undo
   and Redo with no pending text to see 220/470 nF.
4. Choose **RC instance: left** in the selector above the canvas and click C:
   it independently shows 1 µF and Base value 0.000001 F. Its original containing
   literal is unavailable for compact editing; inspection does not create one.
5. Choose **RC instance: right** and click C. Save Copy to a new leaf such as captions-test.json, then
   explicitly Open that copy and select C to inspect its persisted 470 nF value.

The optional local launcher START_RC_CAPTIONS_DEMO.cmd is retained under
build/sn023-rc-captions. This is a development executable requiring the installed
kit, not packaging. Older demo roots/processes and user files remain intact;
choose a different fresh root if the proposed root already exists.

## Proposed next gate and pending behavior

Gather feedback, then separately gate compact resistance editing using the
already supported containing-RC literal operation. General quantity entry,
component/library/Preview properties, positions/rotation/wiring/pan/zoom/arbitrary
shapes, instruments, managed Save, real-engine workers/production IPC/general
execution, keyboard/accessibility/monitor/session/packaging remain pending.

SN-017 Python preparation/GDB/fixture control, accepted profiles and numerical
tolerances, MCU/toolchain independence, PDF auto-open suppression and PID retry
behavior remain unchanged. SN-021 stays one Windows/local-NTFS managed document
and explicit fixed E-01 RC replay with inert Open; external overwrite, automatic
executable resources, production Save, general MCU/mixed-signal execution,
power-loss durability, trusted origin and redistribution remain unproven.
SN-022 accepted only nine synthetic process cases, nine layout checks and a
Qt-free headless worker. Real-engine integration and production IPC remain
pending, and scripted retries do not validate human recovery. SN-044 ownership
is retained; January stabilization and February 2027 classroom use remain
planning targets.
