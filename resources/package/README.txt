SimNodus - interactive circuit and STM32 simulator for teaching
================================================================

This is a development build. Run simnodus.exe; no installation, account or
network access is needed.

What works in this build
- Main window with the component tree, the schematic canvas and the
  properties panel. Drag the splitters to resize the columns.
- View > Components / Properties / Messages (Ctrl+1 / Ctrl+2 / Ctrl+3) collapse
  and reopen the panels at their previous size; F11 focuses the canvas.
- Schematic canvas: mouse wheel zooms at the cursor, the middle button or
  Space+drag pans, F or Home fits the sheet.
- Window > Signal Analyzer (Ctrl+Shift+A) opens the analyzer window, which can
  be moved to another monitor. Window positions and panel sizes are restored
  at the next start.
- Tools > Check Simulation Engine loads the bundled ngspice engine and solves
  a reference circuit; the result appears in the Messages panel.

Placing components, wiring, running simulations and the oscilloscope arrive
in release 0.1 (planned for 2026-10-30).

Command line
  simnodus.exe --check-engine   load ngspice and solve a reference circuit
  simnodus.exe --smoke-test     open the windows, run built-in checks, exit
  simnodus.exe --settings FILE  use FILE instead of the user's settings

Settings are stored in %APPDATA%\SimNodus\SimNodus.ini.

Licenses: see LICENSE (SimNodus, MIT) and THIRD-PARTY-NOTICES.txt.
Project: https://github.com/RicardoKers/SimNodus
