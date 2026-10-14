# Applications

`simnodus/` is the Qt 6 Widgets desktop application (ADR 0081): the Circuit
Editor main window with component tree, schematic canvas and properties
columns, and the separate Signal Analyzer window. Everything except `main()`
is built into `simnodus_ui` so that [GUI tests](../tests/gui) use the same
code.

Command-line options: `--check-engine`, `--smoke-test`, `--screenshot FILE`,
`--settings FILE` (see `simnodus --help`).
