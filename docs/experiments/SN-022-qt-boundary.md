# SN-022: bounded Qt shell and worker experiment

Date: 2026-09-30. Status: **bounded module/worker selection accepted;
production desktop integration and usability remain pending**.

## Question and predeclared acceptance

Can the installed Qt 6 Widgets kit express the existing SN-044 desktop shell
while retaining a document and the last committed fixture observation when a
separate native worker fails? This tests a presentation/process boundary, not
simulation correctness. [ADR 0078](../decisions/0078-qt-widgets-worker-boundary.md)
records the module and crash-handling choice; the
[inventory](../development/QT_INVENTORY.md) records licensing scope.

The [standalone experiment](../../tests/experiments/qt-boundary/README.md) must:

- Open independent editor/analyzer top-level windows with a central placeholder,
  separate Components/Preview and Properties, adjustable panels and retained layout.
- Keep the GUI event loop active while the Qt-free child is working or stalled.
- Accept only complete bounded fixture output and retain an immutable committed
  observation independently of analyzer visibility.
- Observe native child failure and a deadline, preserve document/committed data,
  report failure and start a fresh child only through an explicit retry operation.
- Exercise failed start and a headless worker invocation, without loading engines,
  opening project resources or changing any production domain/application code.

Thresholds in this fixture apply only to wall-clock test completion and GUI
responsiveness. They never replace virtual-time capabilities or the accepted
10 microvolt/1 ps numerical bounds.

## Toolkit assessment against existing criteria

| SN-044 requirement | Widgets fit | Evidence boundary |
|---|---|---|
| Independent editor and analyzer | Two QMainWindow instances | Local shell experiment; no circuit editor or analyzer implementation |
| Components/Preview distinct from instance Properties | Dock widgets and vertical splitter | Placeholder content; no asset loading or execution |
| Resize, hide/reopen and restore panels | QDockWidget, QSplitter and saved main-window state | In-memory layout round trip; durable workspace persistence remains pending |
| GUI refresh separate from simulation | Asynchronous QProcess and presentation timer | Synthetic worker; real integration is separate |
| Shared committed measurements | Immutable Qt-free fixture observation consumed by views | No primary instrumentation store, decoder or plotting system implemented |
| Multiple monitors, DPI and keyboard usability | Toolkit candidates available | Physical usability/recovery acceptance pending |

Quick/QML was evaluated from the same requirements and official module
documentation; no equivalent Quick prototype or comparative performance benchmark
was run. Widgets minimizes new implementation for this shell. This is a bounded
engineering choice, not a universal toolkit ranking.

## Scope and next step

The experiment uses original MIT fixture code and locally installed Qt. No Qt
source, DLL, font, model or vendor asset is incorporated into the repository.
No engine is run or revalidated: unchanged SN-017/SN-021 integration evidence
is reused with its original limits, failures and inconclusive observations.

SN-022's selection acceptance is complete: a small UI experiment, licensing
inventory and crash-handling decision are present. This closes only that bounded
selection task. SN-023/SN-024 still need actual editing, shared instrumentation,
keyboard/accessibility and monitor/DPI restoration evidence. Production worker
integration and binary packaging remain separate work. Reuse this shell when
useful; do not build a generic IPC framework or repeat unchanged backend matrices.

## Measured local result

The [evidence summary](evidence/SN-022-qt-boundary-summary.json) retains exact
commands, source/binary/log hashes, both runtime reports and preservation checks.
The standalone C++20 Release build used MSVC 19.51.36246.0, Windows SDK
10.0.26100.0 and the existing shared Qt 6.11.1 MSVC x64 kit on Windows
10.0.26200. Qt's producer compiler is separately recorded in the inventory.
The platform was native `windows`, with the built-in Fusion style, not offscreen.

| Check | Observation |
|---|---|
| Two windows and layout | Nine assertions passed: independent visible windows, panel hiding/reopening, saved width/splitter/window-size restoration, and analyzer close/reopen without losing the fixture observation |
| Initial run and fresh retries | Three successful complete fixed records; eight started child PIDs across the matrix were distinct |
| Native child crash | Windows access violation `0xC0000005` after a partial record; GUI survived and retained the document and prior committed fixture observation |
| Stalled child | Precise 1,200 ms timer requested termination; terminal callback observed at 1,202 ms; partial output did not commit |
| Invalid outcomes | Failed start, incomplete clean exit, trailing bytes and 1,024-byte output rejected; retained output capped at 128 bytes per stream |
| Event-loop progress | Every started-worker case observed at least 32 timer ticks; maximum measured gap was 44 ms for a requested 10 ms timer |
| Headless independence | Direct worker invocation passed with Qt environment variables removed and PATH restricted to Windows System32; PE imports contained no Qt |

All nine runtime cases passed, with unchanged synthetic document bytes and valid
or preserved immutable fixture snapshots. Independent inspection matched the
four compiled source hashes, all reported assertions and eight nonzero PIDs.
This is evidence about the shell/process boundary; no Renode or ngspice code ran.
The fixed record is canned data, not a simulated electrical measurement.

Fresh retries are named, predeclared harness operations. The matrix automatically
advances between cases; it does not implement or validate a user-click retry flow.
The document is a preset in-memory fixture, not a real editor or SN-021 Save path.
The timer result is a local observation, not a product latency guarantee or a
load/backpressure benchmark. QProcess's internal allocation is not bounded by the
fixture's retained-buffer limit. There is no child-tree, GUI-crash, power-loss,
long-session or multi-monitor durability claim.

## Retained unsuccessful attempts

Two captured initial configure attempts failed before compiler detection because
inherited `PATH` and `Path` keys caused an MSBuild environment exception. An
attempt to use a separate Python executable was denied by the local sandbox.
The successful configuration normalized the child environment through the
available Python 3.14.4 interpreter; no compiler installation was changed.

The first native runtime matrix **failed**: Windows CRT stdout converted LF to
CRLF, so the exact successful frame was rejected. Its other observations cannot
establish preservation of a previously committed snapshot, because that initial
commit did not occur. The raw report, source/binary snapshots and hashes remain
separate negative evidence. The corrected worker explicitly uses binary stdout;
the final matrix also requires a non-null prior snapshot on failure paths and
uses a precise timer. No failure was relabeled as a pass.

## Repository and publication boundary

Started on `codex/sn-022-qt-boundary` from main
`4e0bf2233f1756db71fac3cbeac444b7f7dec13e`, after confirming PR #69's squash,
both successful Foundation checks and matching local/origin/remote main. There
were no unpublished commits. Twelve independent SN-045 working-tree overlays
were backed up and excluded from this publication; all 198 preexisting evidence
files retain their bytes. The source PR records required checks and the eventual
squash identity. No binary, service, release or issue synchronization is included.
