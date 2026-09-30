# SN-022 Qt presentation / worker experiment

Standalone C++20 fixture for the [desktop UX criteria](../../../docs/architecture/DESKTOP_UX.md).
This is a toolkit and process-boundary experiment, not the editor, a simulation
service, a project loader or shared instrumentation implementation. The fixed
document bytes and snapshot are owned synthetic test data. No backend, firmware,
external project or resource is opened. No production target depends on Qt here.

## Build and run on the measured Windows host

Installed kit: Qt 6.11.1 `msvc2022_64`, shared Release libraries. Build: CMake
4.2.3-msvc3, Visual Studio 18 2026 generator, MSVC 19.51.36246.0, Windows SDK
10.0.26100.0, x64. No dependency download or deployment is performed. The optional
`SN022_BUILD_PRESENTATION=OFF` CMake switch omits `find_package(Qt6)` entirely.

From the repository root in PowerShell, choose a new report filename for each run:

```powershell
$cmake = 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' $cmake -S tests/experiments/qt-boundary -B build/sn022/qt-build -G 'Visual Studio 18 2026' -A x64 '-DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64'
python -c 'import os,subprocess,sys; sys.exit(subprocess.call(sys.argv[1:],env=dict(os.environ)))' $cmake --build build/sn022/qt-build --config Release
$env:PATH = 'C:/Qt/6.11.1/msvc2022_64/bin;' + $env:PATH
$env:QT_QPA_PLATFORM = 'windows'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:/Qt/6.11.1/msvc2022_64/plugins/platforms'
$worker = (Resolve-Path build/sn022/qt-build/Release/sn022_worker.exe).Path
python -c 'import os,subprocess,sys; sys.exit(subprocess.run(sys.argv[1:],env=dict(os.environ),timeout=20).returncode)' build/sn022/qt-build/Release/sn022_presentation.exe --worker $worker --report build/sn022/new-runtime-report.json
```

The Python wrapper normalizes this host's inherited duplicate `PATH`/`Path`
environment keys, which otherwise make MSBuild fail before compilation. It is
build/run scaffolding only. The actual presentation and worker are native C++.
Use the installed kit/toolchain paths for another host. `windows` is deliberate:
an offscreen plugin would not establish the measured native window behavior.
The fixture shows two windows briefly, completes its scripted checks, writes
create-only JSON plus stdout, and exits 0 only when every assertion passes.

The independent headless worker can also be run directly:

```powershell
build/sn022/qt-build/Release/sn022_worker.exe success
```

It emits the exact LF-framed 43-byte fixture record and requires no Qt. The
measured headless run additionally removed every `QT_*` environment variable and
restricted `PATH` to `C:/Windows/System32`; PE imports contained no Qt library.
Windows and Microsoft C++ runtime dependencies still apply.

## Acceptance and failure boundary

The presentation links only Qt Core, Gui and Widgets. `QMainWindow`, `QDockWidget`
and `QSplitter` cover two independent top-level windows, resizable side panels,
preview separation, panel hiding/reopening, and in-memory layout/geometry
restoration. Closing and reopening the analyzer retains the controller-owned
immutable snapshot; the view never owns the data. Labels are placeholders.

`QProcess` asynchronously supervises one non-Qt worker at a time. A 10 ms GUI
timer must tick at least twice during every started-worker case; elapsed times,
timer gaps and process IDs are reported. This demonstrates event-loop progress,
not rendering smoothness or product latency. Failed startup may finish immediately.
Each case has a 1200 ms precise timer; expiration kills the child, and only its
terminal signal completes the case. There are no GUI-thread blocking waits.
A 15 s harness watchdog plus a 1 s final escape bounds fixture failure handling.

The private test framing accepts exactly one fixed record after normal exit 0
and empty stderr. Accepted and retained stdout/stderr are capped at 128 bytes
each; incomplete, trailing and oversized output cannot replace the snapshot.
QProcess internal buffering is not a production memory/backpressure guarantee.
The nine scripted operations are initial success, native crash after a partial
record, fresh retry, timeout after a partial record, missing executable, incomplete
clean exit, trailing output, oversized output, and a final fresh retry. Windows
`RaiseException(EXCEPTION_ACCESS_VIOLATION)` supplies the native crash; `SetErrorMode`
suppresses its interactive crash dialog. The timeout separately tests termination.

Every failure checks the same retained non-null immutable snapshot and unchanged
document bytes. The UI marks the retained observation historical/unavailable.
Retries are explicitly named **scripted test operations**, automatically advanced
by the harness, not an implemented user confirmation or recovery interaction.
No automatic continuation of simulation state is implemented.

## Measured results and limits

Local native run `build/sn022/runtime-attempt-02.json` passed all nine operations
and nine layout assertions. The native crash reported `0xC0000005`; the timeout
terminated at 1202 ms. Started-worker cases observed 32-120 heartbeat ticks;
the largest observed GUI timer gap was 44 ms. These are one-host observations,
not acceptance tolerances for future simulation or a scheduling guarantee.

The preceding native run failed because Windows CRT text output changed LF
framing to CRLF. Its report, original source/binary snapshots and hashes remain
under `build/sn022/runtime-attempt-01*`. The correction selects binary stdout;
the parser remains exact. Failed compiler discovery logs retain the duplicate
environment-key problem separately from runtime evidence. No earlier failure
is reclassified as passing.

Pending: human usability, keyboard/accessibility, real editing, monitor removal,
multiple DPI/monitor behavior, persisted cross-session layout, plot rendering,
real committed instrumentation, production IPC/backpressure/cancellation,
descendant-process cleanup, real-backend crashes and clean-machine packaging.
Process separation is not a security sandbox or durable recovery mechanism.
Toolkit selection grants no simulation capabilities or full-editor scope.
