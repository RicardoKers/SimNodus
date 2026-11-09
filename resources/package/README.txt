SimNodus - interactive circuit and STM32 simulator for teaching
================================================================

Release 0.17 (analog and digital circuits, the Blue Pill / STM32F103
microcontroller, AVR chips and the Arduino UNO, and an experimental
ESP32-C3 board with Wi-Fi).
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

New in 0.17: energy and motor control. Solar panels (single-diode model
from datasheet values, irradiance and temperature set with the mouse) with
an MPPT example; lithium-ion and LiFePO4 batteries (1-4 cells, with or
without protection) with a CC/CV charger example; experimental BLDC and
PMSM motors with a three-phase bridge (with current sense), six-step and
field-oriented control examples on a Blue Pill; a two-axis joystick and a
load set with the mouse on the DC motor. Sensors (LM75, DS18B20, DHT22,
HC-SR04) are set with the mouse while running; capacitors and inductors
take an initial voltage and current; the Signal Analyzer has Pause; a
wire's segment can be dragged alone; a new ESP32-C3 starts without firmware
and waits for an upload. Every example has a note on its diagram.

New in 0.16 (experimental): Wi-Fi for the ESP32-C3 boards. They join
SimNodus's virtual access point (SimNodus / simnodus-wifi, WPA2) and talk to
each other; Simulate > Wi-Fi Network... sets the network. A gateway, off by
default and confirmed on a project's first run, lets the boards reach this
computer (host.simnodus) and the internet: web pages, MQTT brokers, NTP
time and ping; forwarded ports listen on 127.0.0.1 only. Example 60 is a
web server on a board. Uploads through the DevKit's UART0 at 921600 baud
are fast and reliable again.

New in 0.15: Simulate > Debug Firmware with GDB works with the AVR chips
and the Arduino UNO through avr-gdb (Microchip Studio, MPLAB XC8, avr-gcc
toolchains): breakpoints, watchpoints on the SRAM and I/O registers,
stepping and load. While a debug run waits, its message names only the
debugger for the circuit's chip and each board's port. The pre-run checks
warn when one net carries two label names.

New in 0.14: AVR microcontrollers in SimNodus's own emulator. The bare
ATmega8 and ATmega8A (PDIP-28) run .hex or .elf firmware from Microchip
Studio, avr-gcc or the Arduino IDE with their fuses (Fuses... dialog), a
Crystal part, the EEPROM kept in the project, timers and PWM, USART, ADC,
SPI, I2C and sleep modes; they were compared with a real chip. The
ATmega328P and ATmega328, the ATmega32 and ATmega32A (PDIP-40) and an
Arduino UNO board with Optiboot are experimental until compared: the UNO
takes uploads from the Arduino IDE through a virtual serial port pair
(examples 50-59, see MICROCONTROLLER.md). The ESP32-C3 gained GDB
debugging, the continuous ADC with DMA, its temperature sensor and
MicroPython's files kept in its flash.

New in 0.13: an ESP32-C3-DevKitM-1 board, experimental until it is compared
with a real board. It runs a flash image (.bin) of ESP-IDF, Arduino or
MicroPython unchanged: GPIO, LEDC PWM, ADC, I2C, SPI, RMT (WS2812B strips),
UART0 and the USB Serial/JTAG port (examples 45-48). Its serial ports can be
linked to the computer, through a virtual serial port pair (com0com) or TCP
(rfc2217://localhost:4000), so the Arduino IDE, Thonny, mpremote and esptool
work with it, uploads included (see MICROCONTROLLER.md). Not yet: Wi-Fi,
Bluetooth, GDB on the ESP32-C3.

New in 0.12: a Serial Port part connects the circuit to a real serial port
of the computer (a USB-serial adapter: choose the port, baud rate and format
in Properties; example 44). While a run is paused, the mouse wheel zooms the
time axis of the scope and the Logic lanes to show a transition in detail.
Channel scales are automatic and follow a signal that leaves the screen.
For video tutorials, scripts control the Signal Analyzer and show a drawn
pointer, key badges and highlights while they work the window's real
buttons and fields (see AUTOMATION.md).

New in 0.11: about 60 new parts for automation projects: sensors (LM35,
NTC, PT100, thermocouple, LDR, ACS712, load cell and HX711, Hall, PIR,
inductive 24 V, 4-20 mA), 74HC logic chips, I2C/SPI/1-Wire chips (BMP280,
MPU6050, DS3231, SSD1306 OLED, MCP3208, W25Q32, microSD, DS18B20, DHT22,
HC-SR04, WS2812B), motors and drivers, power and interface chips, and
buzzers you can hear (Simulate > Sound). File > Save with Firmware;
examples 36-43.

Community: questions, circuits and bug reports are welcome on the SimNodus
Discord server, https://discord.gg/AsjGNtMnef (English or Portuguese), and
on https://github.com/RicardoKers/SimNodus/issues. Video tutorials:
https://www.youtube.com/@SimNodus.

New in 0.10: the user guides in Brazilian Portuguese (folder pt-BR; Help >
User Guide opens them when the interface is in Portuguese). Counters,
flip-flops and decoders simulate 2-3x faster; every example was checked as
a student would use it.

New in 0.9: the firmware runs at the speed of the real chip (Cortex-M3
instruction timing with the flash wait states, within 5 % of a Blue Pill;
Simulate > Simulation Settings can turn it off). Messages can be copied (Ctrl+C);
selected wires and probes show their values while running; the meters show
their polarity; fixes for adjustable sources and the simple transistors.

New in 0.8: AI agents such as Claude Code or Codex build, check and
simulate circuits through simnodus-cli.exe, also step by step in the open
window for video tutorials (Tools > Allow External Control), and through
its MCP server (simnodus-cli mcp); see AUTOMATION.md. External I/O parts
exchange signals with a virtual plant over TCP (Simulate > External I/O).
A 555 timer, and any vendor .SUBCKT as a part (Place > Subcircuit).

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
https://github.com/RicardoKers/SimNodus/tree/main/docs/user). The same
guides in Brazilian Portuguese are in pt-BR (Início rápido, Guia do usuário,
Microcontrolador); Help > User Guide opens them when the interface is in
Portuguese. Lessons 1-3
are in examples/lessons; examples 17-23 show gates, buses, diode kinds,
blocks, a counter, regulators and a 7-segment display; examples 24-28 the
RTC and watchdogs, SPI and I2C chips and a character LCD; examples 29-33
the new sources, the keypad, the converters, a ROM and a RAM; examples
34-35 a 555 timer and a vendor subcircuit.

Command line
  simnodus.exe FILE.snx         open a project
  simnodus.exe FILE.snx --run   open and start the simulation
  simnodus.exe --check-engine   load ngspice and solve a reference circuit
  simnodus.exe --check-mcu-engine  run blink.elf in the microcontroller emulator
  simnodus.exe --smoke-test     open the windows, run built-in checks, exit
  simnodus.exe --settings FILE  use FILE instead of the user's settings
  simnodus.exe --control        allow external control by AI agents
  simnodus-cli.exe ...          SimNodus without a window (see AUTOMATION.md)

Settings are stored in %APPDATA%\SimNodus\SimNodus.ini.

Licenses: see LICENSE (SimNodus, MIT), THIRD-PARTY-NOTICES.txt and licenses/.
