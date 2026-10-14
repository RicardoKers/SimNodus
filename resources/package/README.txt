SimNodus - interactive circuit and STM32 simulator for teaching
================================================================

Development build of release 0.1. Run simnodus.exe; no installation,
account or network access is needed.

Start with File > Examples, press F5 to run, and click switches, hold push
buttons or drag potentiometers while the simulation runs. Window > Signal
Analyzer (Ctrl+Shift+A) shows the probed signals.

The quick-start guide is QUICK-START.md in this folder (also on
https://github.com/RicardoKers/SimNodus/blob/main/docs/user/QUICK_START.md).

Command line
  simnodus.exe FILE.snx         open a project
  simnodus.exe FILE.snx --run   open and start the simulation
  simnodus.exe --check-engine   load ngspice and solve a reference circuit
  simnodus.exe --smoke-test     open the windows, run built-in checks, exit
  simnodus.exe --settings FILE  use FILE instead of the user's settings

Settings are stored in %APPDATA%\SimNodus\SimNodus.ini.

Licenses: see LICENSE (SimNodus, MIT), THIRD-PARTY-NOTICES.txt and licenses/.
