# ADR 0081: Desktop UI technology and layout

Date: 2026-10-14. Status: **accepted**.

## Context

The owner asked for a QSpice-like clean editor: component tree left, schematic
center, properties right, adjustable widths, and analysis in a separate window
([DESKTOP_UX](../architecture/DESKTOP_UX.md)). ADR 0078 already selected Qt 6
Widgets. Plotting and symbol rendering libraries were not selected.

## Decision

- Qt 6 Widgets (Core, Gui, Widgets; Svg only if needed for icons). Main window
  with `QSplitter` columns; Signal Analyzer as a second top-level `QMainWindow`.
- Schematic canvas with `QGraphicsScene`/`QGraphicsView`; one graphics item per
  component, wire, label and probe; edits through `QUndoStack` commands over the
  Qt-free document model.
- Plots drawn with a project-owned `QPainter` widget using min/max decimation;
  no Qt Charts/Qt Graphs/QCustomPlot (GPL licensing incompatible with a
  permissive release, and limited control of roll/trigger behavior).
- Symbols are data files rendered by one generic painter, enabling a later
  Symbol Editor.
- Settings and window/panel geometry in `QSettings`.
- Supported Qt: 6.5 LTS or newer (CI on Linux uses the distribution Qt; Windows
  uses the owner's installed Qt 6.11 MSVC kit or a CI-installed kit).

## Alternatives

Qt Quick/QML (richer animation, more tooling, weaker fit for dense dockable
technical UI); web front-ends (extra runtime, no benefit for offline desktop).

## Consequences

Plot features (trigger, cursors, measurements) are implemented in-house and
tested. Layout details of ADR 0049 are superseded by this ADR; its separate
analyzer window and shared-measurement principles remain.

## Revisit criteria

Measured rendering performance below NF-03 targets.
