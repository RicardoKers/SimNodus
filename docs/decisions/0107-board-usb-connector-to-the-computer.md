# ADR 0107: A board's USB connector reaches the computer as a serial link

Date: 2026-10-29. Status: accepted (tasks SN-252, SN-253, SN-254).

## Context

The ESP32-C3's two serial ports (UART0 behind the DevKit's USB-serial bridge,
and the chip's own USB Serial/JTAG) only reached SimNodus's terminal. The
owner wants the Arduino IDE, Thonny, mpremote and esptool to work with a
simulated board as with a real one: the serial monitor, the MicroPython REPL,
and uploading firmware.

## Decision

- A board's UART can have a **link to the computer**, chosen in a part
  parameter (`uart0_link`, `usb_link` on the ESP32-C3-DevKitM-1): a serial
  port of the computer, or `tcp:PORT`. The profile names the parameter and
  the board's reset circuit for each UART (`link_parameter`,
  `reset_circuit`). No USB protocol is simulated: on the computer both ports
  are a COM port anyway, so the link carries bytes and the program's DTR and
  RTS.
- **Serial port**: one end of a virtual port pair the user installs
  (com0com, VSPE), opened by SimNodus with SN-237's code; the program opens
  the other end. The program's DTR and RTS arrive as DSR and CTS.
- **TCP**: a server on 127.0.0.1 only, one program at a time, speaking RFC
  2217 (Telnet COM port control) when the program starts with a Telnet
  command (pyserial's `rfc2217://`, so esptool and mpremote need no driver),
  raw bytes otherwise (`socket://`, a raw TCP terminal).
- The link lives in the engine's MCU runner (`src/devices/host_link`,
  Qt-free), polled at most every millisecond of the computer's time, like the
  Serial Port part (ADR 0105); it opens with the run and closes with it.
- **Reset circuit** (`McuMachine::host_lines`): `auto_reset` is the DevKit's
  two transistors (RTS without DTR pulls EN low; DTR without RTS pulls GPIO9
  low; EN rises 50 ms later); `usb_jtag` is the USB Serial/JTAG's logic (RTS
  without DTR holds the chip in reset; DTR without RTS before it chooses
  download mode). A program sets DTR and RTS in two calls, so the board sees
  a new state only after it held for 5 ms.
- The emulator holds the chip while EN is low and samples GPIO9 when it
  starts; with GPIO9 low the unmodified ROM runs its download mode, and
  esptool's flasher stub writes the flash. For that the UART got autobaud
  (RXD_CNT, LOWPULSE, HIGHPULSE), received bytes arrive one frame time apart
  at the configured baud rate (SN-254), the core takes an interrupt three
  instructions after its line rises (the ROM enables a UART interrupt one
  store before it says which port it serves), SPI1 user reads without
  USR_COMMAND send USER2's command (the ROM's reads), and the data bus past
  SRAM1 reads 0 (the stub reads a status mask there).
- A flash written after a download boot is the board's memory 0 in the run;
  when the run stops the window saves it as `<designator>-flash.bin` beside
  the project and makes it the board's firmware, as a real board keeps what
  was uploaded.

## Alternatives

- **Our own virtual COM port driver**: a kernel driver to sign and ship; out
  of scope.
- **Simulating USB (CDC-ACM) for a USB/IP client**: the program would see a
  real USB device, but it needs a USB/IP client driver on Windows and far
  more work for the same bytes.
- **The window owns the link** (Qt Serial Port, Qt Network): a relay through
  the engine protocol, a new Qt module, and no link in `simnodus-cli` runs.

## Consequences

- The Arduino IDE and Thonny only list COM ports: they need a virtual port
  pair. esptool and mpremote work over TCP without one.
- Uploads are as fast as the simulation: at real-time speed esptool at
  921600 baud writes 200 KB in about 3 s.
- UART0 and UART1 work at the pin level (SN-254): frames leave bit by bit on
  the TX signal and the RX signal is decoded once its line was idle for a
  frame, so the Serial Port part and other boards reach GPIO21/GPIO20. The
  boot log takes its real time at 115200 baud (about 0.2 s before an
  application starts). RX from a pin whose edges only come at the circuit's
  steps is decoded only when its bits are long enough, as on the STM32.
- Real-port behavior (com0com's line mapping) can only be checked on a
  computer with the pair installed; the automated tests use TCP.
