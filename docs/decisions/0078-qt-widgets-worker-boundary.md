# ADR 0078: Qt Widgets shell and a separate simulation worker

Date: 2026-09-30. Status: **accepted for bounded SN-022 module/worker selection;
production integration and desktop acceptance remain pending**.

## Context

[SN-044](0049-desktop-ux-and-measurements.md) already selected independent
Circuit Editor and Signal Analyzer windows, adjustable panels, preview before
insertion and shared measurements. SN-022 evaluates how to present that design;
it does not reopen it or authorize a complete editor. A native backend library
can crash its host, so moving the solver to a GUI-owned thread is insufficient
to preserve the document when the process fails.

## Decision

Select **Qt 6 Widgets**, with **Core, Gui and Widgets**, for the initial Windows
desktop shell. Use two `QMainWindow` instances, dock panels and splitters. Keep
Qt in presentation and the desktop process adapter. The installed shared Qt
6.11.1 MSVC x64 kit is the experiment baseline, not a release version commitment.
See the [module/license inventory](../development/QT_INVENTORY.md) and
[bounded experiment](../experiments/SN-022-qt-boundary.md).

Qt Quick/QML remains an alternative if a later measured rendering need justifies
its scene graph and additional module/tooling dependencies. It has no measured
advantage for this small desktop shell. No plotting package, Qt Charts, Qt Graphs,
WebEngine, network protocol, dependency manager or static Qt distribution is
selected here. Plotting requires its own rendering and licensing evaluation.

Use an **out-of-process simulation worker** for future native-engine execution.
The desktop process may use asynchronous `QProcess` notifications; it must not
wait synchronously for the solver in the GUI thread. The experimental worker
is a Qt-free C++ fixture, not Renode/ngspice integration. Its private standard
output protocol is disposable evidence, not the production Automation API.
Qt Core supplies QProcess; a network module is unnecessary for this experiment.

The desktop process retains the editable document in application/domain state;
presentation owns view state. Application code owns commands and run lifecycle;
the existing kernel remains the sole simulation-time authority.
Instrumentation owns committed captures, retention and distribution. Adapters
own engine-specific details. Neither window becomes a capture store, time owner
or direct backend caller. A Qt event-loop timer measures responsiveness only;
it does not advance simulated time. The worker must also remain usable headless.

## Failure and recovery policy

On failed start, malformed/incomplete output, crash or missed deadline, mark the
run failed/unavailable and fence further replies from it. Retain the editable
document and previously committed observations with their original provenance;
show that they are historical, not current live values. Never promote tentative
output just because the worker exited. Bound messages and retained diagnostic
output, reap the failed child, and require an explicit fresh run after failure.
Do not resume a failed engine or automatically replay side-effecting commands.

The small experiment tests local child termination and explicit fresh retry.
Production child-tree ownership (including Renode), transport backpressure,
run IDs, committed-trace transfer and GUI-exit teardown still require integration
evidence. Process isolation is crash containment, not a security sandbox or a
durable capture mechanism. No Qt call changes SN-021 resource/execution authority.
This simulation worker is not the SN-021 managed-save authority or a persistence
service; selecting it does not resolve production managed-store provisioning.

## Alternatives and consequences

| Alternative | Assessment for this slice |
|---|---|
| Solver on GUI thread | Blocks input/event handling; rejected |
| Solver on another thread in the GUI process | May keep input responsive, but native process failure still loses both windows and unsaved edits; rejected as crash boundary |
| Separate worker with Widgets presentation | Small match to existing desktop criteria; selected for local measurement |
| Quick/QML or mixed Widgets/Quick | Viable future rendering option; defer until demonstrated need |

No production application, managed-save service, full editor or expanded runtime
profile is delivered. SN-017 retains Python preparation, GDB transport and fixture
sequencing. Accepted numerical tolerances, MCU/toolchain independence, PDF
auto-open suppression, PID retry behavior, SN-044 and independent SN-045 work
remain unchanged. SN-021's [bounded acceptance](../experiments/SN-021-final-acceptance.md)
is not broadened by an additional process.

## Revisit criteria

Revisit toolkit choice only for measured canvas/trace rendering or accessibility
limitations. Validate monitor removal, mixed DPI, keyboard use and real-engine
lifecycle before claiming desktop readiness. Review the exact packaged module,
plugin and runtime dependency closure before any binary distribution. Keep
January stabilization and the February classroom target as planning constraints,
not consequences proven by this experiment.
