# E-10: AVR emulator, ATmega8 first — results

Date: 2026-11-02. Tasks: SN-255 (prerequisites, recorded in the plan), SN-256
(core). Result so far: **gate G10a met**. Gates G10b and G10c have not run.
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

## Not covered yet

The ATmega8 machine, its peripherals, fuses and integration (SN-257,
SN-258; gate G10b) and the complete chip (SN-259; gate G10c).
