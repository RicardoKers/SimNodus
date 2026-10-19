SimNodus - interactive circuit and STM32 simulator for teaching
================================================================

Release 0.3 (analog and digital circuits and the Blue Pill / STM32F103
microcontroller).
Run simnodus.exe; no installation, account or network access is needed.

Start with File > Examples, press F5 to run, and click switches, hold push
buttons or drag potentiometers while the simulation runs. Window > Signal
Analyzer (Ctrl+Shift+A) shows the probed signals, the logic lanes of the
microcontroller pins and the serial terminal.

Microcontrollers: a Blue Pill runs your firmware (an ELF file, for example
from STM32CubeIDE) in SimNodus's own STM32 emulator: GPIO, EXTI, USART
(also receiving from the circuit), timers and PWM, ADC, DMA. Simulate >
Debug Firmware with GDB lets STM32CubeIDE debug the firmware while it drives
the circuit.

New in 0.3: logic gates; buses with splitters (B, S); text notes on the
schematic (T); model variants of transistors, MOSFETs and diodes in
Properties > Model, and your own .model files in Tools > Open Models Folder;
the potentiometer shows its position; Export VCD in the Logic tab for
PulseView or GTKWave.

Guides in this folder: QUICK-START.md and MICROCONTROLLER.md (also on
https://github.com/RicardoKers/SimNodus/tree/main/docs/user). Lessons 1-3
are in examples/lessons; examples 17 and 18 show gates and buses.

Command line
  simnodus.exe FILE.snx         open a project
  simnodus.exe FILE.snx --run   open and start the simulation
  simnodus.exe --check-engine   load ngspice and solve a reference circuit
  simnodus.exe --check-mcu-engine  run blink.elf in the microcontroller emulator
  simnodus.exe --smoke-test     open the windows, run built-in checks, exit
  simnodus.exe --settings FILE  use FILE instead of the user's settings

Settings are stored in %APPDATA%\SimNodus\SimNodus.ini.

Licenses: see LICENSE (SimNodus, MIT), THIRD-PARTY-NOTICES.txt and licenses/.
