SimNodus - interactive circuit and STM32 simulator for teaching
================================================================

Release 0.7 (analog and digital circuits and the Blue Pill / STM32F103
microcontroller).
Run simnodus.exe; no installation, account or network access is needed.
The setup.exe of the same release installs SimNodus with a Start menu entry
and opens .snx files with a double-click.

Start with File > Examples, press F5 to run, and click switches, hold push
buttons or drag potentiometers while the simulation runs. Window > Signal
Analyzer (Ctrl+Shift+A) shows the probed signals, the logic lanes of the
microcontroller pins and the serial terminal.

Microcontrollers: a Blue Pill runs your firmware (an ELF file, for example
from STM32CubeIDE) in SimNodus's own STM32 emulator: GPIO, EXTI, USART
(also receiving from the circuit), timers and PWM, ADC, DMA, SPI, I2C, RTC,
watchdogs and CRC. Simulate >
Debug Firmware with GDB lets STM32CubeIDE debug the firmware while it drives
the circuit.

New in 0.7: adjustable and controlled sources; a 4x4 matrix keypad; A/D and
D/A converters of 4 to 12 bits; parallel ROM and RAM of 16 bytes to 64 KB,
also without a microcontroller (Window > Memory Viewer shows them).

New in 0.6: chips for microcontrollers (74HC595, MAX7219 with an LED
matrix, 24C02 EEPROM, LM75 sensor, PCF8574 expander, HD44780 character LCD);
SPI and I2C; Window > Memory Viewer shows, loads and saves the EEPROM's
contents; brighter LEDs.

New in 0.5: simulations run in their own program (simnodus-engine.exe), so
an engine crash ends only the run and SimNodus stays open; the installer
(setup.exe); the interface in Brazilian Portuguese (Tools > Language); a dark
theme (View > Theme); the user guide (Help > User Guide).

Guides in this folder (also Help > User Guide): QUICK-START.md,
USER-GUIDE.md with the known limitations, and MICROCONTROLLER.md (also on
https://github.com/RicardoKers/SimNodus/tree/main/docs/user). Lessons 1-3
are in examples/lessons; examples 17-23 show gates, buses, diode kinds,
blocks, a counter, regulators and a 7-segment display; examples 24-28 the
RTC and watchdogs, SPI and I2C chips and a character LCD; examples 29-33
the new sources, the keypad, the converters, a ROM and a RAM.

Command line
  simnodus.exe FILE.snx         open a project
  simnodus.exe FILE.snx --run   open and start the simulation
  simnodus.exe --check-engine   load ngspice and solve a reference circuit
  simnodus.exe --check-mcu-engine  run blink.elf in the microcontroller emulator
  simnodus.exe --smoke-test     open the windows, run built-in checks, exit
  simnodus.exe --settings FILE  use FILE instead of the user's settings

Settings are stored in %APPDATA%\SimNodus\SimNodus.ini.

Licenses: see LICENSE (SimNodus, MIT), THIRD-PARTY-NOTICES.txt and licenses/.
