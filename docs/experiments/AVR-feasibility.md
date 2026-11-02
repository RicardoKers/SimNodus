# AVR feasibility study: ATmega8, ATmega32, ATmega328P and the Arduino UNO

Date: 2026-11-02. Status: **study only**; nothing is implemented. Plan: [E-10](E-10-plan.md). Asked by the
owner because students are building projects with Atmel (Microchip) AVR
microcontrollers. If the owner goes ahead, this study becomes the plan of an
experiment (E-10) with gates recorded before any emulator code, as E-07 and
E-08 did.

## Question

Can SimNodus run unmodified firmware for the ATmega8, ATmega32 and
ATmega328P (the PDIP parts students put on a breadboard: ATmega8A-PU,
ATmega32A-PU, ATmega328P-PU) and for the Arduino UNO, with the circuit, the
serial port to the computer, the usual upload tools and GDB, as it does for
the Blue Pill and the ESP32-C3?

## Short answer

Yes, and it is smaller work than the ESP32-C3 was. The AVR is an 8-bit
core with a fixed, fully documented instruction set and peripherals described
register by register in public datasheets. There is no ROM, no flash cache,
no bootloader chain from the vendor, and no radio. 16 MHz is far below what
SimNodus's interpreters already reach (about 47 MIPS on the ESP32-C3 machine
with all its peripherals), so real time should be easy.

The integration work done for the ESP32-C3 is reused as it is:
- the `McuMachine` interface (ADR 0106);
- the GDB server with its `GdbTarget` interface (SN-249);
- the serial host links with DTR/RTS reset circuits (ADR 0107), which is
  exactly how the UNO's auto-reset works;
- firmware written by the chip kept in the project (SN-248), which would
  hold the EEPROM.

The pin model already takes the supply as a parameter, so 5 V parts need
only their values.

The three chips share one core (AVRe+, with MUL). They differ in memory
sizes, vector tables and peripherals, so one machine described by a table
per chip covers them. The Arduino UNO is the ATmega328P on a board, plus
auto-reset, the bootloader upload and the board drawing.

## The parts

| | ATmega8 (A) | ATmega32 (A) | ATmega328P |
|---|---|---|---|
| Package | PDIP-28 | PDIP-40 | PDIP-28 |
| Flash / SRAM / EEPROM | 8 KB / 1 KB / 512 B | 32 KB / 2 KB / 1 KB | 32 KB / 2 KB / 1 KB |
| JMP/CALL, vector size | no (RJMP only), 1 word | yes, 2 words | yes, 2 words |
| Ports | B, C (PC0-6), D | A, B, C, D | B, C (PC0-6), D |
| Timers | T0 8-bit (no compare, no PWM), T1 16-bit, T2 8-bit async | T0 8-bit with PWM, T1 16-bit, T2 8-bit async | T0, T2 8-bit with 2 PWM each, T1 16-bit |
| External interrupts | INT0, INT1 | INT0, INT1, INT2 | INT0, INT1, pin change on every pin (PCINT) |
| ADC | 10-bit, ADC0-5 in PDIP (ADC6/7 only in TQFP) | 10-bit, 8 channels, differential with gain | 10-bit, ADC0-5 in PDIP, temperature sensor |
| Internal reference | 2.56 V | 2.56 V | 1.1 V |
| USART | 1, UBRRH/UCSRC share an address (URSEL bit) | 1, same URSEL quirk | USART0, separate registers |
| Others | SPI, TWI, analog comparator, WDT | SPI, TWI, comparator, WDT, JTAG | SPI, TWI, comparator, WDT with interrupt, debugWIRE |
| Factory clock | internal RC 1 MHz | internal RC 1 MHz | internal RC 8 MHz ÷ 8 (CKDIV8) = 1 MHz |

Points that matter for students, and that a faithful model must reproduce:
- **ATmega32 JTAG.** It ships with JTAG enabled (JTAGEN fuse), so PC2-PC5
  do not work as GPIO until the fuse is changed or the firmware sets JTD
  twice. This is a classic classroom surprise; the simulator should
  reproduce it and say why in its log.
- **Factory clock.** A new chip runs at 1 MHz. Code built with
  `F_CPU=16000000` runs 16 times slower until the fuses select the crystal.
  The fuses are part settings (and avr-libc's `FUSES` section in the ELF
  can set them), shown in the part's properties.
- **Part names.** "ATmega328-PU" without the P is a different chip with
  another signature (0x1E9514 instead of 0x1E950F). The Arduino IDE and
  avrdude refuse it unless told otherwise. The library would offer the
  328P (what the UNO uses) and could add the 328 as a signature variant.
- **Pins.** The ATmega8 in PDIP has no ADC6/ADC7.

## What would be built

1. **AVR core** (`src/emu/avr_cpu.*`, Qt-free like the others):
   - the AVRe+ instruction set (about 130 instructions) with the cycle
     counts of the AVR Instruction Set Manual;
   - the 32 registers and the I/O registers mapped into data space;
   - SREG and the interrupt rules: latency 4 cycles, one instruction runs
     after RETI and after SEI;
   - sleep, SPM self-programming (the page buffer and the RWW and NRWW
     sections, which the bootloader needs), and the boot section with
     BOOTRST and IVSEL.

   Exact cycle counts matter: `_delay_ms` and Arduino's `delayMicroseconds`
   count cycles.
2. **ATmega machine** (`src/emu/atmega.*`): one class parametrized by a chip
   table (memory sizes, vectors, register addresses, which peripherals
   exist). The peripherals:
   - GPIO with pull-ups (and PUD), external interrupts and PCINT;
   - Timer0/1/2 with all PWM modes, input capture and the 16-bit TEMP
     register, plus Timer2 asynchronous with a 32.768 kHz crystal;
   - the USART with the URSEL quirk on the older chips;
   - the ADC (sample and hold, 13 ADC clocks per conversion, references,
     the 328P's temperature sensor);
   - the analog comparator, SPI, TWI and WDT;
   - the EEPROM with its write time (about 3.4 ms) and the four-cycle
     EEMPE window;
   - fuses and lock bits, clock sources, prescaler (CLKPR), brown-out and
     reset sources (MCUSR).
3. **McuMachine adapter** `AvrMachine`. Firmware formats:
   - Intel HEX (what Microchip Studio, MPLAB X and the Arduino IDE's
     *Export Compiled Binary* produce);
   - ELF, which carries symbols for GDB and the `.eeprom` and `.fuse`
     sections;
   - raw `.bin`.
4. **Library parts**:
   - `atmega8_pdip28`, `atmega32_pdip40` and `atmega328p_pdip28`, which
     are bare chips with VCC, GND, AVCC, AREF, RESET and XTAL pins. Their
     netlists hold a RESET pull-up and the supply checks, and the clock
     comes from the fuses;
   - `arduino_uno`, the board with its 5 V and 3.3 V rails, the LED on
     pin 13 (D13/PB5), the reset button and the auto-reset capacitor
     driven by the host link's DTR.
5. **Upload and serial**:
   - The UNO's USART0 goes through the host link: a COM pair or RFC 2217,
     as with the ESP32-C3.
   - With DTR → 100 nF → RESET and the Optiboot bootloader in the chip,
     **the Arduino IDE's Upload button and avrdude work unchanged** over
     the virtual port, the same way esptool works with the ESP32-C3.
   - The bare chips take their firmware from the Firmware parameter, as
     the Blue Pill does. ISP programmers (USBasp) are not emulated.
6. **GDB**:
   - An `AvrTarget` for the existing server: r0–r31, SREG, SP and PC, with
     gdb's AVR address spaces (flash at 0, SRAM at 0x800000, EEPROM at
     0x810000), plus breakpoints, watchpoints and steps.
   - It works with avr-gdb (Microchip's AVR toolchain, PlatformIO).
   - Microchip Studio debugs only through its own debugWIRE and JTAG tools,
     so it would not attach. It also has its own simulator, without the
     circuit.

## Licences (all compatible, nothing GPL linked)

- **Own code.** The core and peripherals are written from Microchip's
  datasheets and the instruction set manual, as the STM32 and ESP32-C3
  emulators were.
- **avr8js** (Wokwi's AVR emulator, MIT). It can be read and compared
  against as a second reference, and its MIT licence would even allow
  porting parts with attribution.
- **simavr** (GPL-3). It can only be an external test oracle, run as a
  separate program like QEMU was for the ESP32-C3, never linked.
- **Optiboot** (the UNO's bootloader, GPL-2). It is firmware that runs
  inside the emulated chip, not code linked into SimNodus. Shipping its
  .hex beside the board (with its source link and licence text) is
  aggregation, like the MicroPython image. **Owner decision:** ship it, or
  ask users to load it. A third option is to answer the STK500 protocol in
  the board model without running a bootloader; that is simpler but not
  faithful, so this study does not recommend it.
- **Other parts of the ecosystem.**
  - The Arduino AVR core is LGPL-2.1: user firmware only, never committed
    as SimNodus code, as with Arduino-ESP32.
  - avr-libc is BSD.
  - avr-gcc and avrdude are GPL tools that the user runs; SimNodus does
    not ship them.

## Validation

Unlike the ESP32-C3, whose boards are still on their way, AVR hardware is
already in the classroom. An Arduino UNO and DIP chips allow board
comparison from the start (GPIO timing, PWM frequencies, ADC readings,
serial output, EEPROM contents after a run).

- **Instruction tests.** One test per instruction with its flags and cycle
  count from the manual. Differential runs against simavr and avr8js on
  generated instruction sequences, as `riscv-diff` does against the
  reference.
- **Firmware tests.**
  - avr-gcc builds (owned sources in `examples/firmware/avr_*`);
  - Arduino sketches exported as `.hex`: Blink, Fade, AnalogReadSerial,
    Servo, tone, EEPROM, Wire to an LM75, SPI;
  - the factory-clock and JTAG cases above.
- **Real time.** The target is at least 1.0× real time at 16 MHz with the
  circuit, measured as E-07 measured the STM32.

## Proposed stages (each a gate, each releasable)

Order set by the owner on 2026-11-02: the ATmega8 first.

1. **Core + ATmega8/8A minimum.**
   - The PDIP-28 parts, fuses with the dialog, the Crystal part.
   - GPIO, INT0/1, Timer0/1/2 and the USART with the URSEL quirk.
   - EEPROM and WDT.
   - `.hex` and ELF loading.

   Gate: owned avr-gcc examples and Microchip Studio `.hex` files from a
   typical course (LED, LCD, keypad, timers, serial), compared with a real
   ATmega8 on a breadboard.
2. **ATmega8 complete.** ADC, analog comparator, SPI, TWI, sleep, Timer2
   asynchronous, brown-out from the VCC pin. Gate: ADC, I2C (LM75) and SPI
   examples.
3. **ATmega328/328P and ATmega32/32A** (chip tables, PCINT, the 328P's
   timers, the ATmega32's JTAG and differential ADC). Gate: the same
   examples rebuilt for each chip.
4. **Arduino UNO.**
   - The board, auto-reset and Optiboot.
   - Upload with the Arduino IDE and avrdude through the virtual port.
   - The EEPROM kept in the project.

   Gate: Blink, Serial, AnalogReadSerial, Servo, tone, EEPROM and Wire,
   compared with a real UNO.
5. **GDB** with avr-gdb.

## Size, compared with the ESP32-C3

The core is smaller than the RV32IMC core: no CSRs, no privilege modes, no
MMU, no PMP. The machine is smaller than the ESP32-C3's:
- no ROM to rebuild;
- no flash cache or MMU;
- no eFuse;
- no second-stage bootloader to boot through.

The peripherals are about as many as the Blue Pill's. The integration
(session, GUI parts, host link, GDB) already exists. As an estimate: about
6–7 PRs at the pace of SN-246..SN-249, the first stage being the largest.

## Fuses and lock bits

On a real chip the fuses decide the clock, the reset vector, the brown-out
level and which pins work, and a wrong fuse is a common classroom problem.
SimNodus treats them as part of the chip, not of the firmware.

**Where they live.** Each chip part has the parameters *Low fuse*, *High
fuse*, *Extended fuse* (328/328P only) and *Lock bits*. They are hex bytes,
stored in the project, with the factory values as defaults (to be checked
against each datasheet):

| Part | Low | High | Extended |
|---|---|---|---|
| ATmega8/8A | 0xE1 | 0xD9 | — |
| ATmega32/32A | 0xE1 | 0x99 | — |
| ATmega328/328P | 0x62 | 0xD9 | 0xFF |
| Arduino UNO | 0xFF | 0xDE | 0xFD |

The UNO's lock byte is 0xCF.

They take effect at power-up and reset, as on the chip. A change during a
run applies at the next reset, and the log says so. The CLI sets them like
any other parameter.

**How they are edited.** The bytes use inverted logic (programmed = 0), the
usual source of mistakes. The part's properties show the raw bytes and a
**Fuses…** dialog that decodes them field by field, in the manner of the
online fuse calculators:
- clock source and start-up time, as one list of the valid CKSEL/SUT
  combinations;
- CKDIV8, BOOTSZ and BOOTRST;
- BODEN/BODLEVEL;
- WDTON, EESAVE, RSTDISBL, JTAGEN, CKOPT, SPIEN.

Both views edit the same bytes. Each field is described per chip in the
library JSON (byte, mask, values, text in English and pt-BR), so one
generic dialog serves every AVR part.

**From the firmware.** An ELF with avr-libc's `.fuse` and `.lock` sections
(the `FUSES` macro) carries the values that Microchip Studio's production
programming writes. When such an ELF is loaded, SimNodus does the same: it
writes the values into the part's parameters, as an edit of the project,
and tells the user in the log. A `.hex` has no fuses, so the part's values
stay.

**What each fuse does in the simulation:**
- **CKSEL/SUT (clock source).**
  - Internal RC: the nominal frequency (1/2/4/8 MHz on the ATmega8/32,
    8 MHz or 128 kHz on the 328/328P). OSCCAL is read with the factory
    calibration value and scales the frequency (approximate, labeled).
  - Crystal or resonator: the frequency of the crystal part connected
    between XTAL1 and XTAL2. This needs a new two-pin *Crystal* part with a
    frequency parameter; the circuit does not simulate the oscillation.
    With no crystal there, the chip does not start, as on a breadboard, and
    the log says why.
  - External clock: the frequency of the source on XTAL1 (a pulse or clock
    source). It is read from the source's parameters, not measured.
  - The start-up time (SUT plus 6, 258, 1K or 16K clock cycles) delays the
    first instruction after power-up.
- **CKDIV8** (328/328P) sets CLKPR's reset value. **CKOUT** is logged as
  not simulated, because a 16 MHz square wave on PB0 is beyond what the
  circuit can follow in real time.
- **BOOTSZ/BOOTRST, IVSEL and the lock bits** (BLB0x/BLB1x) are
  implemented: SPM and LPM are refused where the lock bits forbid them, as
  Optiboot expects. LB (read-out protection) affects only programmers,
  which are not emulated, so it is shown but has no effect.
- **EESAVE.** Loading a new firmware into a bare chip is a chip erase plus
  programming. The EEPROM is erased unless EESAVE is programmed or the
  firmware carries its own `.eeprom` data. An upload through the UNO's
  bootloader never erases the EEPROM, as on the board.
- **BODEN/BODLEVEL.** The chip reads the voltage on its VCC pin from the
  circuit. Below the brown-out level it is held in reset (MCUSR.BORF).
  Without brown-out detection, running below the datasheet's minimum for
  the frequency gives a warning in the log; the chip does not fail
  randomly.
- **WDTON** keeps the watchdog always on. **RSTDISBL** turns PC6 into an
  I/O pin. **JTAGEN** (ATmega32) takes PC2–PC5 until the firmware sets JTD
  twice.
- **Accepted but without effect:** SPIEN and DWEN/OCDEN. ISP and on-chip
  debugging are not emulated, and GDB uses SimNodus's own server.
- **Reading fuses.** The firmware can read the fuse and lock bytes with
  LPM after setting BLBSET (`boot_lock_fuse_bits_get`). The signature and
  calibration bytes can be read the same way.

## Owner's decisions (2026-11-02)

- Optiboot is shipped with the UNO, with its licence and source link.
- The ATmega8 comes first. Stage 1 becomes **core + ATmega8**; the
  ATmega328/328P and the UNO follow.
- The ATmega328 (without P) and the "A" versions are separate parts:
  ATmega8, ATmega8A, ATmega32, ATmega32A, ATmega328, ATmega328P.
  - The 8/8A and the 32/32A share a signature; the 328 (0x1E9514) and the
    328P (0x1E950F) do not.
  - The A versions differ mainly in their electrical tables (supply range
    and speed grades).

## Risks and open points

- **Analog accuracy.** The ADC's input model (sample-and-hold capacitor,
  source impedance) and the 5 V pin model need values from the datasheet's
  electrical tables. They should be labeled approximate until measured on
  a board.
- **Timer2 asynchronous mode** needs a second clock domain. It is used by
  RTC lessons, so it should not be dropped.
- **Clock from the circuit.** Simulating the crystal oscillator in the
  circuit is out of scope. The clock comes from the fuses and the crystal
  part's frequency (see Fuses and lock bits).
