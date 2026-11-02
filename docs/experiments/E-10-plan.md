# E-10: AVR emulator, ATmega8 first — plan

Date: 2026-11-02. Tasks: SN-255..SN-262 in the [backlog](../planning/BACKLOG.md).
Status: **planned**; no emulator code yet. The criteria below are recorded
before any code, as for [E-07](E-07-results.md) and [E-08](E-08-plan.md),
and are not relaxed after a run: a failed criterion leads to a recorded
decision. Background, the parts compared and the fuse design:
[AVR feasibility study](AVR-feasibility.md).

## Question

Can SimNodus's own emulator run **unmodified** ATmega8 firmware (the `.hex`
and ELF files that avr-gcc, Microchip Studio and MPLAB X produce) in the
circuit, with the chip's fuses, cycle-exact timing, the EEPROM kept between
runs, faster than real time, and without changing anything that the STM32
and ESP32-C3 paths do today?

## Why the ATmega8 first

Owner's decision of 2026-11-02: students build their projects on bare AVR
chips. The ATmega8 is the smallest of the three chips, so the shared core
and the machine's chip table are proven on it first. The ATmega32 and
ATmega328/328P are then mostly tables, and the Arduino UNO is the
ATmega328P on a board.

Other decisions of the same day:
- Optiboot is shipped with the UNO.
- The ATmega8, ATmega8A, ATmega32, ATmega32A, ATmega328 and ATmega328P are
  separate parts.

## Scope

**In (G10a–G10c):**
- the AVRe+ core with exact cycle counts;
- the ATmega8's peripherals;
- fuses and lock bits as part parameters, with the **Fuses…** dialog;
- a two-pin Crystal part;
- the ATmega8-PU and ATmega8A-PU parts in PDIP-28 with their power pins
  and the 5 V pin model;
- Intel HEX and ELF loading;
- a command-line runner (`simnodus-emu --avr atmega8`);
- integration as a board through the `McuMachine` interface (ADR 0106),
  marked **experimental** until the board comparison passes.

**Out (later stages, SN-260..SN-262):** the ATmega32/32A and
ATmega328/328P, the Arduino UNO with Optiboot and upload, and GDB.
Also out: ISP programmers (USBasp), debugWIRE and JTAG debugging, the
oscillator simulated in the circuit, and CKOUT.

## Prerequisites (SN-255, before emulator code)

1. **Pinned references.**
   - The ATmega8A datasheet (Microchip), the older ATmega8 datasheet
     (Atmel) for the differences between the two parts, and the *AVR
     Instruction Set Manual* (Microchip). Each is identified by document
     number and revision in the report.
   - The cycle counts used are the manual's AVRe column.
2. **Toolchain.**
   - avr-gcc and avr-libc as the Arduino AVR core installs them through the
     arduino-cli already in `build/tools`, with the version recorded.
   - Microchip's AVR 8-bit toolchain for the Microchip Studio files.
   - Every image used is identified by SHA-256 in the report.
3. **Oracles.**
   - **simavr**, built in `build/tools` and run as a separate, test-only
     process. It is GPL-3: never linked, never copied, its source not read
     while writing the corresponding model.
   - **avr8js** (MIT) may be read and its instruction tests compared.
   - The owner's **ATmega8A-PU** on a breadboard, with a USB-serial adapter
     and an oscilloscope or logic analyzer, is the final reference. The
     firmware is written with the owner's programmer.
4. **Course material.** The owner supplies two or three Microchip Studio
   projects from a typical course (LED, LCD, keypad, timers, serial), with
   their `.hex` and ELF files. They become test material only with the
   owner's permission, and otherwise are tested by hand.

## Prerequisite results (SN-255, 2026-11-02)

**References.**

| Document | Number and revision |
|---|---|
| ATmega8A datasheet, complete (Microchip) | DS40001974B (2020) |
| ATmega8 datasheet (Atmel, for the non-A part) | 2486AA-AVR-02/2013, to be confirmed with the PDF |
| AVR Instruction Set Manual (Microchip) | DS40002198C (11/2024) |

The PDFs are not stored in the repository; their numbers are enough to find
them on Microchip's site.

**Toolchain.**
- avr-gcc **7.3.0** (`7.3.0-atmel3.6.1-arduino7`) and avr-libc **2.0.0**,
  installed with the Arduino AVR core **1.8.8** through the arduino-cli in
  `build/tools/arduino-cli`. Its data folder is `D:\data`, outside the
  repository, as for the ESP32 core.
- avrdude **8.0.0-arduino1** comes with it, for the owner's USBasp.
- This is the compiler that Arduino IDE users have. Microchip Studio users
  have Microchip's toolchain; their `.hex` files are tested when the owner
  provides some.
- Checked: a blink built with `-mmcu=atmega8` and avr-libc's `FUSES`
  macro gives a `.fuse` section at 0x820000 holding 0xE1, 0xD9: the factory
  values of the table in the study. `.eeprom` is at 0x810000 and `.lock`
  at 0x830000, the addresses the ELF loader reads.

**Oracle (changed before any comparison ran).** The plan named simavr.
This laptop has no Linux subsystem, and MSYS2 has no simavr package;
building it for Windows is not worth the risk. The primary differential
oracle is therefore **avr8js 0.21.1** (MIT, npm, run with Node.js, which is
installed), as a test-only tool in `build/tools/avr8js`.
- Being MIT, its source may also be read.
- The difference rule of G10a.2 is unchanged: when avr8js and SimNodus
  disagree, the manual or the chip decides.
- simavr stays optional, for example on Linux CI later.
- Installed on 2026-11-02 with the owner's permission. A first run (LDI,
  ADD, ADIW) gives the right results and 4 cycles.
- **Known deviation, found in that first run.** avr8js computes the
  half-carry flag H from bit 0 instead of bit 3 in ADD, ADC, SUB, SUBI,
  SBC, SBCI, CP, CPC, CPI and NEG. In ADD, for example, the code reads
  `1 & ((d & r) | (r & ~R) | (~R & d))`, while the manual gives
  H = Rd3·Rr3 + Rr3·¬R3 + ¬R3·Rd3.
  - Example: 5 + 5 gives H = 1 in avr8js, where the chip gives 0; 8 + 8
    gives H = 0, where the chip gives 1.
  - The manual decides, so the differential test compares H against the
    manual's formula, not against avr8js. Every other flag is compared
    with avr8js as planned.
  - H matters only for BCD arithmetic, which is probably why it went
    unnoticed. Reporting it upstream is the owner's call.

**Hardware (the owner's, for G10b.9).** An ATmega8 (the marking says
whether it is the ATmega8-16PU or the ATmega8A-PU; the comparison uses that
part), a USBasp programmer, a USB-serial adapter, an oscilloscope and a
logic analyzer. The firmware is written with avrdude and the USBasp.
- Fuses are written only to the values the comparison needs. The factory
  values give 1 MHz internal.
- For a 16 MHz crystal the ATmega8 needs CKSEL=1111 and CKOPT programmed:
  low 0xEF, high 0xC9. The values are checked against the datasheet before
  they are written: a crystal fuse without a crystal stops the chip until a
  clock is supplied.

**Course projects.** None exist yet. The owner's courses use digital I/O,
analog input, PWM output, serial communication and timers to keep
everything in step. The owned firmware set below covers the same, and it
is also the material of the board comparison (G10b.9) and of the examples.

## Owned firmware set

Sources in `examples/firmware/avr_*`, built with the pinned avr-gcc. Each
one is a `.c` file with a README, plus the `.hex` and ELF it was built to.
Each runs at the factory 1 MHz or at 16 MHz with a crystal, chosen by
`F_CPU` and the `FUSES` in the source.

| Firmware | Covers | Compared on the board (G10b.9) |
|---|---|---|
| `avr_blink` | `_delay_ms`, a digital output | period |
| `avr_button` | a digital input with pull-up, INT0 on the falling edge, debounce with Timer0 | response to a press, seen on the analyzer |
| `avr_pwm` | Timer1 Fast PWM on OC1A/OC1B, Timer2 PWM on OC2 | frequency and duty |
| `avr_serial` | USART at 9600 baud with the RX interrupt: a prompt, echo and simple commands | bytes in the terminal |
| `avr_adc` | ADC0 single conversion with AVCC; ADC1 free running; values sent over serial | readings against a known voltage (G10c.1) |
| `avr_station` | everything together, kept in step by a Timer1 CTC tick of 1 ms: read the potentiometer, set a PWM duty from it, blink a status LED, report over serial every 100 ms | the whole trace: serial lines, PWM, LED period |

`avr_station` is the model of a student project. Its trace from the
simulator and from the board, taken with the logic analyzer on TXD, OC1A
and the LED, is the comparison that matters most.

## Work packages

| # | Package | Done when |
|---|---|---|
| 1 | `src/emu/avr_cpu.*`: AVRe+ core for a flash of up to 64 K words. Includes: the 32 registers, I/O and SRAM in one data space, SREG, SP, interrupts with their latency, the instruction after RETI and SEI, SLEEP, WDR, SPM and LPM, BREAK. Predecoded instructions as in E-07 | Instruction tests pass |
| 2 | Instruction tests (each instruction's result, flags and cycles from the manual) in ctest; random differential blocks against avr8js, batched like `riscv-diff` | Results recorded |
| 3 | Benchmark firmware (busy loop, a mix with MUL, 16-bit arithmetic, loads and stores) | Numbers recorded → **G10a** |
| 4 | `src/emu/atmega.*`: the machine parametrized by a chip table, with the ATmega8 table. Peripherals: GPIO with pull-ups and PUD; INT0/1; Timer0 (overflow only), Timer1 (all modes, OC1A/B, ICP1, 16-bit TEMP), Timer2 (synchronous); USART with the URSEL quirk, frames bit by bit on its pins; EEPROM with its write time and EEMWE window; WDT and the timed change sequence; MCUCSR reset sources; the boot section with BOOTRST, IVSEL and the lock bits | Owned firmware runs in the CLI |
| 5 | Fuses: decoding (clock source, SUT start-up, CKOPT, BODEN and BODLEVEL, BOOTSZ, BOOTRST, EESAVE, WDTON, RSTDISBL), with each field described per chip in the library JSON. The ELF's `.fuse`, `.lock` and `.eeprom` sections. Intel HEX loading | Fuse tests pass |
| 6 | Integration: an `AvrMachine` adapter; parts `atmega8_pdip28` and `atmega8a_pdip28` (VCC, GND, AVCC, AREF, RESET, XTAL1/2; 5 V pin model with values from the datasheet's tables); the Crystal part; the **Fuses…** dialog (English and pt-BR); the EEPROM kept like the ESP32-C3's flash (SN-248); examples and guides | GUI test and examples pass → **G10b** |
| 7 | ATmega8 complete: ADC (sample and hold, references, 13 or 25 ADC clocks), analog comparator, SPI, TWI, sleep modes with time skipping, Timer2 asynchronous with a 32.768 kHz crystal, brown-out from the VCC pin | → **G10c** |
| 8 | Report `docs/experiments/E-10-results.md` | Gates reviewed with the owner → decision **DE10** |

## Gate G10a — core

| ID | Criterion |
|---|---|
| G10a.1 | Every instruction the ATmega8 has: result, every SREG flag and the cycle count equal the Instruction Set Manual, including skips over one- and two-word instructions and the 16-bit operations (ADIW, SBIW, MOVW, MUL family) |
| G10a.2 | Random differential against avr8js (simavr in the first version of this plan; see the prerequisite results). Every decoder entry is hit at least 1 000 times, with operands that include r0/r1 (MUL's result), the X, Y and Z pointers with pre-decrement and post-increment, and carries across 8 and 16 bits. Zero unexplained differences: a difference counts as an oracle deviation only when a third reference (the manual or the chip) agrees with our result |
| G10a.3 | At least 64 MIPS on the busy loop and 32 MIPS on the benchmark mix, Release build, on the owner's laptop on Windows (MSVC); Linux (GCC) recorded on CI. That is 4× and 2× real time at 16 MHz. SLEEP lets waiting firmware run even faster |
| G10a.4 | All 65 536 16-bit opcodes decode without crashing the host. An opcode the ATmega8 does not have gives a diagnostic with the PC and runs as on the chip where the chip's behavior is documented, otherwise as a NOP, as listed in the report. 200 random flash images run 100 000 cycles each without a crash or hang |

## Gate G10b — ATmega8 firmware in the circuit

| ID | Criterion |
|---|---|
| G10b.1 | Owned avr-gcc firmware: a blink with `_delay_ms`. The period in virtual time equals the cycle count of the compiled code exactly, at the factory 1 MHz and at 16 MHz with a Crystal part and its fuses |
| G10b.2 | Timer1: a CTC interrupt and Fast PWM on OC1A. The frequency and duty on the pin equal the datasheet's formulas exactly in cycles. Timer0 overflow and Timer2 CTC give their periods exactly |
| G10b.3 | USART at 9600 and 38 400 baud (U2X on and off). The frames on TXD reach the Serial Port part and the terminal byte for byte, and an RX interrupt echoes what the terminal sends |
| G10b.4 | INT0 on a falling edge from a button in the circuit, and the level-triggered mode; the latency is 4 cycles plus the instruction in progress |
| G10b.5 | EEPROM: a value written by the firmware is there after stop and start, and after closing and reopening the project. Loading a new firmware erases it unless EESAVE is programmed or the firmware carries `.eeprom` data |
| G10b.6 | Fuses. The factory values 0xE1/0xD9 give 1 MHz internal. A crystal selected with no Crystal part connected: the chip does not start and the log says why. WDTON forces the watchdog on; RSTDISBL turns PC6 into an I/O pin. An ELF with `FUSES` sets the part's fuses and the log says so. The firmware reads its fuse and lock bytes with LPM |
| G10b.7 | Resets: power-on, the RESET pin from the circuit, the watchdog (MCUCSR's PORF, EXTRF, WDRF) and a jump to the boot section with BOOTRST |
| G10b.8 | The owner's course `.hex` files (LED, LCD with the HD44780 part, keypad with the 4×4 part) run unchanged |
| G10b.9 | Board comparison with the owner's ATmega8A-PU on at least three firmware. Same serial output byte for byte; blink period and PWM frequency within the crystal's tolerance; PWM duty within one timer count; the factory-clock 1 MHz blink within the RC oscillator's datasheet tolerance (the simulator uses the nominal frequency, labeled) |
| G10b.10 | Two runs of the same firmware with the same scripted inputs give identical traces (hash) |
| G10b.11 | Nothing changes for the STM32 and the ESP32-C3. Every existing ctest suite, `tools/examples_pass.py` and `tools/examples_gui_pass.py` pass unchanged. Their MIPS in `emu-benchmark` stay within 3 %. The AVR code lives in its own files, and nothing GPL is linked |

## Gate G10c — ATmega8 complete

| ID | Criterion |
|---|---|
| G10c.1 | ADC single conversion and free running. With AVCC and the internal 2.56 V references, the result is ⌊Vin·1024/Vref⌋ ±1 LSB for a source with low impedance. The conversion takes 13 ADC clocks (25 for the first). Labeled approximate until compared with the owner's chip (±2 LSB) |
| G10c.2 | The analog comparator, with its interrupt and Timer1's input capture from it |
| G10c.3 | SPI master to the 74HC595 and MAX7219 parts. TWI master to the LM75 and 24C02 parts, with owned firmware using avr-libc's `<util/twi.h>` |
| G10c.4 | Timer2 asynchronous with a 32.768 kHz Crystal on TOSC1/2: a 1 s tick exact in virtual time, and the wake from power-save |
| G10c.5 | Sleep modes. Idle and power-down are woken by INT0 and the watchdog, and the run skips the sleeping time (not slower than real time) |
| G10c.6 | Brown-out. A VCC source lowered below the level selected by BODEN/BODLEVEL (2.7 or 4.0 V) holds the chip in reset with BORF set. Without brown-out detection, a supply below the datasheet's minimum for the clock gives a warning in the log |

## Outcomes (decision DE10)

- **G10a and G10b pass.** An ADR for AVR on the own emulator. The
  ATmega8/8A ship as experimental parts until G10b.9, then G10c and the
  next stages.
- **Only G10a.3 (speed) fails.** A week for performance, then re-review.
- **A G10b criterion fails twice.** The experiment stops with its measured
  reasons, and the owner decides.

## Later stages (tentative, detailed after DE10)

- **SN-260, ATmega328/328P and ATmega32/32A.** Chip tables, JMP/CALL and
  2-word vectors, PCINT, the 328's Timer0/2 PWM, the 1.1 V reference and
  temperature sensor, CKDIV8 and CLKPR, the ATmega32's JTAG fuse taking
  PC2–PC5 and its differential ADC with gain, INT2, the PDIP-40 part.
- **SN-261, Arduino UNO.**
  - The board: 5 V and 3.3 V rails, the LED on D13, the reset button, and
    DTR → 100 nF → RESET on the host link (ADR 0107).
  - Optiboot shipped with its licence and source link, and the board's
    fuses and lock bits.
  - The Arduino IDE's Upload and avrdude unchanged through the virtual
    port, and the EEPROM kept.
  - Arduino sketches as test material, never the LGPL core committed as
    SimNodus code.
- **SN-262, GDB.** An `AvrTarget` for the existing server, with avr-gdb's
  address spaces (flash at 0, SRAM at 0x800000, EEPROM at 0x810000).

## Risks

1. **Analog accuracy.** The ADC input model and the 5 V pin model use
   datasheet tables; they are labeled approximate until compared with the
   owner's chip.
2. **Timer2's asynchronous clock domain** and its register update
   handshake (ASSR busy flags).
3. **Undocumented behavior** (unused opcodes, reserved bits). It is
   recorded in the report, never guessed silently.
4. **Course projects** may use parts the library does not have yet; each
   one becomes a library task, not a blocker of the gate.

## References

- ATmega8A and ATmega8 datasheets (Microchip, Atmel).
- AVR Instruction Set Manual (Microchip).
- avr-libc documentation (`<avr/fuse.h>`, `<avr/boot.h>`, `<util/twi.h>`).
- simavr (test-only oracle, GPL-3): https://github.com/buserror/simavr
- avr8js (MIT): https://github.com/wokwi/avr8js
- Optiboot (GPL-2, shipped as firmware for the UNO):
  https://github.com/Optiboot/optiboot
