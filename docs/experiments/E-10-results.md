# E-10: AVR emulator, ATmega8 first — results

Date: 2026-11-02. Tasks: SN-255 (prerequisites, recorded in the plan), SN-256
(core), SN-257 (the ATmega8 machine). Result so far: **gate G10a met**; the
machine passes its tests from the command line, and gate G10b is judged after
the integration in the circuit (SN-258) and the board comparison (G10b.9).
Gate G10c has not run.
Plan and predeclared criteria: [E-10-plan.md](E-10-plan.md); they are not
repeated here.

## Environment and provenance

- **Machine.** Windows 11, MSVC 19.51.36246 (Release, `/O2 /Ob2`), on the
  owner's laptop. Linux: GCC 13 in CI.
- **References.** AVR Instruction Set Manual DS40002198C; ATmega8A datasheet
  DS40001974B.
- **Oracle.** avr8js 0.21.1 (MIT), run with Node.js 24.19.0.
  - Its `dist/cjs/cpu/instruction.js` has SHA-256
    `a80cb750d6bab9b7ba64410280f9d87c2a9d70cb069a258091d74967b63a22b2`.
  - It runs with the five corrections listed below.
- **Corpus.** `tests/experiments/avr-differential`, from `generate.py`'s
  default seed 2026:

  | File | SHA-256 |
  |---|---|
  | `blocks.bin` | `97a06ff242e0b9795269ef66a5169f5428177855c4192a79f2c97e587f36bfbd` |
  | `avr8js.bin` | `0e7983f426bacaa256568c8c8e2f67582eb9f13d05459f8dc355d8f1bc235b91` |

## What was built

`src/emu/avr_cpu.*` (no Qt, no third-party code) is the AVRe+ core for flash
of up to 64 K words. It has:
- predecoded instructions, cached per flash word and dropped when the word
  is written;
- one data space: the registers, SREG and SP in the core, the other I/O
  registers through the machine's `read_io`/`write_io`, and the SRAM;
- the cycle counts of the manual's AVRe column;
- interrupts:
  - the machine reports a pending one and the core takes the lowest
    vector;
  - 4 cycles, plus 4 when waking from sleep;
  - one instruction runs after SEI and after RETI before the next
    interrupt;
  - IVSEL through `set_vector_base`;
- hooks for SLEEP, WDR, SPM, BREAK and the LPM reads of fuse, lock and
  signature bytes;
- diagnostics for opcodes the chip lacks (run as NOPs) and for accesses
  outside the data space.

The chip's differences sit in `AvrConfig`:
- flash and data sizes and the end of the I/O space;
- SP's reset value and its width (11 bits on the ATmega8);
- JMP/CALL and BREAK present or not;
- 1- or 2-word vectors.

## Gate G10a

| ID | Result |
|---|---|
| G10a.1 | **Met.** `emu-avr-unit`, 12 cases. Results, all six arithmetic flags and cycles of every ATmega8 instruction, with the edge values of each formula: H from bit 3 (5+5 gives H=0, 8+8 gives H=1), Z carried through SBC/SBCI/CPC, V of NEG 0x80 and of INC/DEC at 0x7F/0x80, ADIW and SBIW leaving H alone, the six multiplications with FMULS's documented overflow. Also every load and store mode with its pointer update, skips over one- and two-word instructions (2 and 3 cycles), the return address on the stack low byte first, RJMP wrapping around the flash, SPH's 11 bits, and the interrupt timing |
| G10a.2 | **Met.** `emu-avr-differential`: 1 500 blocks, 0 differences. Every operation the blocks may use ran at least 1 000 times. The six left out (JMP and CALL, which the ATmega8 lacks; SLEEP, WDR, SPM and BREAK, which the machine handles) are in the directed cases |
| G10a.3 | **Met.** `emu-avr-benchmark` on the owner's laptop, three runs: 120.0–121.9 MIPS on the busy loop (SUBI, SBCI, BRNE: about 10× real time at 16 MHz) and 139.7–140.7 MIPS on the mix (LD X+, LDD, MUL, ADD, ADC, ST Z+, ANDI, ADIW, CPI, BRNE: about 13.5× real time). Targets: 64 and 32 MIPS. The core alone, without a machine; G10b.11 measures it with the ATmega8's peripherals |
| G10a.4 | **Met.** All 65 536 opcodes decode to an operation of size 1 or 2; 1 895 are not instructions of the ATmega8 and run as NOPs with a diagnostic. 200 random 8 KB images run 100 000 cycles each without a crash or hang |

## avr8js deviations found

Each deviation was found by a difference in the corpus, confirmed with the
manual, and corrected in the runner's copy of avr8js. Nothing in SimNodus
changed to match avr8js.

1. **H, the half carry, from bit 0 instead of bit 3.** Ten instructions:
   ADD, ADC, SUB, SUBI, SBC, SBCI, CP, CPC, CPI and NEG. The manual gives
   H = Rd3·Rr3 + Rr3·¬R3 + ¬R3·Rd3 for ADD.
2. **SBIW changes H.** The manual says SBIW does not affect it.
3. **NEG never sets V.** avr8js compares `0 - Rd`, a negative number in
   JavaScript, with 128.
4. **CBI takes 1 cycle.** The ATmega8's datasheet and the manual's AVRe
   column give 2, as avr8js's own SBI does.
5. **FMULSU sets Z when the result is not zero.** FMUL and FMULS are right.

The corpus also avoids two properties of avr8js that are not deviations for
its own chip:
- it does not wrap the PC around the flash;
- it keeps a 16-bit SP.

Reporting these to avr8js is the owner's decision.

## The ATmega8 machine (SN-257)

`src/emu/atmega.*` puts the core in a machine built from the ATmega8A
datasheet: the ports, INT0/INT1, the three timers, the USART, the EEPROM, the
watchdog, the resets, self-programming and the fuses. Time is counted in CPU
cycles at the clock the fuses select. Every peripheral with events at known
cycles stops the core at its next one, so the effects come at the exact
cycle:
- a timer at its next compare match, TOP, BOTTOM or overflow;
- the USART at each bit;
- the EEPROM at the end of a write;
- the watchdog at its time-out.

What it covers:
- **Timers.** They count in jumps between events, not tick by tick. Each
  event happens at the tick that leaves its counter value, as in the
  datasheet's timing diagrams. Covered:
  - Timer1's sixteen modes (one reserved), with the double-buffered OCR1x,
    the 16-bit TEMP register, input capture and its noise canceler's four
    cycles;
  - Timer2's four modes, and Timer0's overflow;
  - the compare outputs' actions in each mode (FOC included);
  - the external clocks T0 and T1;
  - the shared prescaler and its reset (PSR10, PSR2).
- **USART.**
  - TXD sends frames bit by bit (5 to 9 data bits, parity, one or two stop
    bits, U2X).
  - RXD is decoded from the pin, sampled in the middle of each bit, with
    frame, parity and overrun errors and the two-byte FIFO.
  - Bytes from the computer arrive one frame time apart.
  - UBRRH and UCSRC share their address: URSEL on write, and two reads in a
    row return UBRRH, then UCSRC.
- **EEPROM.** The EEMWE window of four cycles, 8.448 ms per write (the
  datasheet's 8448 cycles of the 1 MHz oscillator), and the CPU halted for 2
  cycles on a write and 4 on a read.
- **Watchdog.** 16K to 2048K cycles of its 1 MHz oscillator, the WDCE
  sequence, and WDTON.
- **Fuses.**
  - The clock source (internal RC at 1, 2, 4 or 8 MHz; a crystal, an
    external clock or an RC from a part parameter).
  - The start-up time from CKSEL and SUT before the first instruction.
  - BOOTSZ and BOOTRST, RSTDISBL (PC6 as I/O), EESAVE on a chip erase,
    and WDTON.
  - With a crystal or an external clock selected and nothing connected, the
    chip does not start and says why.
  - CKOPT: a note when a crystal above 8 MHz runs without it.
- **Self-programming.** SPM from the boot section (the page buffer, erase
  and write in 4.1 ms, the boot lock bits), and LPM of the fuse and lock
  bytes after BLBSET.
- **Firmware.** Intel HEX (checksums verified) and avr-gcc ELF, with the
  `.eeprom`, `.fuse` and `.lock` sections applied as a production
  programmer writes them.

**Not yet:**
- Simulated in SN-259, and noted in the log once if the firmware uses them:
  the ADC, the analog comparator, SPI, TWI, sleep modes other than Idle,
  Timer2's asynchronous mode, brown-out, and OSCCAL.
- Not modelled at all: the input synchronizer's 1.5-cycle delay on PINx,
  and the alignment of a transmitted frame to the baud generator (up to one
  bit).

**Owned firmware** (`examples/firmware/avr_*`, built with avr-gcc 7.3.0
by `tools/build_avr_firmware.py`, reproducible byte for byte):

| Firmware | What it does |
|---|---|
| `avr_blink` | Blinks the LED on PB0, in a 1 MHz build and a 16 MHz build |
| `avr_button` | INT0 with a debounce from a Timer1 CTC tick |
| `avr_pwm` | Timer1 fast PWM, Timer2 phase-correct PWM, Timer0's overflow |
| `avr_serial` | The USART with an echo, and a counter kept in the EEPROM |

`emu-atmega8` (ctest, 6 cases) runs them and checks:

| Criterion | What `emu-atmega8` checks |
|---|---|
| G10b.1 | The blink at both clocks: the 1 MHz period repeats exactly, and both periods are `_delay_ms`'s cycles plus the loop's few. The first edge comes after the start-up times (6 CK + 65 ms; 16K CK + 4.1 ms) |
| G10b.2 | The PWM periods and duties exact in cycles: 2000 and 500/1500 for Timer1, 4080 and 1024 for Timer2. Timer0's overflow every 16 384 cycles |
| G10b.3 | The greeting byte for byte, bits of 104 cycles on TXD, the echo through the RX interrupt, and a frame decoded from RXD |
| G10b.4 | INT0 on the falling edge, with a bounce ignored |
| G10b.5 | The EEPROM over a reset, and erased or kept by EESAVE on a chip erase |
| G10b.6 | The factory fuses at 1 MHz, a crystal with none connected, WDTON, RSTDISBL, the fuses taken from the ELF, and the fuse bytes read with LPM |
| G10b.7 | The power-on, RESET-pin and watchdog resets (PORF, EXTRF, WDRF) |

The criteria that need the circuit, the project or the owner's chip are open:
G10b.3's Serial Port part, G10b.5's project, G10b.8 and G10b.9. So is the
speed with the machine (G10b.11). From the command line, `simnodus-emu --avr`
ran `avr_blink` for 10 s of simulated time in 0.071 s (112 MIPS).

## In the circuit (SN-258, first part)

`AtmegaMachine` (`src/mcu/mcu_machine.cpp`) puts the machine behind the
runner. The parts `atmega8_pdip28` and `atmega8a_pdip28`
(`tools/generate_avr_parts.py`) place it in the circuit. Their electrical
model:
- each GPIO drives through the runner's pin model at 5 V, 35 Ohm out,
  with a 35 kOhm pull-up, and reads 1 above 3.0 V and 0 below 1.0 V (the
  ATmega8A datasheet at 5 V);
- VCC draws about 2 mA;
- PC6 has the RESET pull-up while it is RESET.

The session passes the fuses, the kept EEPROM file and the frequency of a
**Crystal** part found between XTAL1 and XTAL2. The run controller keeps the
EEPROM as `<designator>-eeprom.bin`, through the memory channel the ESP32-C3's
flash uses (SN-248).

Results:
- **Session test** (`engine-ngspice`, examples 50 and 51): the greeting and
  the echo through the terminal, the EEPROM holding the count, and the
  crystal's 16 MHz found in the circuit.
- **GUI test** (`gui-editor`): a saved project keeps `U1-eeprom.bin`, and the
  next run continues the count.
- **`tools/examples_pass.py`**: example 50 at 3.17x real time, 51 at 2.01x.
  The LED of example 51 blinks at 1.000 Hz, with a high level of 4.72 V under
  its load.

This covers G10b.3's terminal and G10b.5's project.

**Second part.** The **Fuses...** dialog (`apps/simnodus/fuses_dialog.*`)
decodes the three bytes field by field and keeps them in step with the hex.
A GUI test checks it with the bytes of example 51 (EF, C9) and a typed E4
(internal 8 MHz).

The plan placed the field descriptions in the library JSON. They are in
the dialog's code instead, so their texts are translated with the
interface. The next chips add their tables there.

When a run starts, an ELF's `.fuse` and `.lock` sections that differ from
the part's properties become the properties, in one undoable edit. A GUI
test checks it with example 51 set back to E1/D9.

Open: G10b.8 and G10b.9, which need the owner's course files and chip.

## Not covered yet

The complete chip (SN-259; gate G10c).
