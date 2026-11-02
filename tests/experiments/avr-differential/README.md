# AVR differential test (E-10)

Compares SimNodus's AVRe+ core (`src/emu/avr_cpu.*`) with avr8js on random
instruction blocks, for gate G10a.2 of the
[E-10 plan](../../../docs/experiments/E-10-plan.md).

- **`generate.py`** writes `blocks.bin`: 1 500 blocks of 128 words. Each
  record holds:
  - random registers, with X, Y and Z pointing into a 128-byte SRAM window
    at 0x100;
  - a random SREG;
  - SP at 0x25F;
  - the window's contents;
  - the code.

  The code uses every ATmega8 instruction except those the machine handles
  (SLEEP, WDR, SPM, BREAK) and the two it lacks (JMP, CALL). The
  [directed cases](../../unit/avr_tests.cpp) cover those. Operands keep the
  behavior defined:
  - no load or store through a pointer into the same register;
  - destinations below r26, so that X, Y and Z move only by their own
    increments;
  - no writes to SPL or SPH with OUT;
  - branches inside the block, and RCALL only forward;
  - IJMP and ICALL through a Z loaded just before them, whose target loads
    Z back into the window;
  - no RET at word 0.

  The same seed gives the same file.
- **`run_avr8js.js`** runs every block in avr8js 0.21.1 (MIT, test-only, in
  `build/tools/avr8js`) and writes `avr8js.bin`. Each record holds:
  - the registers, SREG, SP and PC;
  - the cycles and steps;
  - the window.

  A run ends when the PC reaches the block's last word, after 400
  instructions, or when SP falls below 0x68 (the I/O registers).
- **`emu-avr-differential`** (ctest) runs the same blocks in SimNodus's core
  and compares the records byte for byte, without Node.js installed. It also
  prints the operations that run fewer than 1 000 times.

```text
python tests/experiments/avr-differential/generate.py
node tests/experiments/avr-differential/run_avr8js.js build/tools/avr8js/node_modules
```

The runner corrects five deviations of avr8js 0.21.1 from the AVR
Instruction Set Manual (DS40002198C) in a copy of its instruction module.
Each correction is checked by count when it is applied, and listed with its
reason at the top of the script:
- the half carry from bit 0 instead of bit 3 in ten instructions;
- SBIW changing H;
- NEG's V;
- CBI's cycles;
- FMULSU's Z.

It also masks SP to the ATmega8's 11 bits after each instruction. A
remaining difference counts as an avr8js deviation only when a third
reference (the manual or the owner's chip) agrees with SimNodus; otherwise
it is an emulator bug.

Trace one block on both sides (the steps and their PC, opcode, cycles, SREG
and SP):

```text
build/dev/tests/unit/avr_tests <repository> "avr-diff" 73
node tests/experiments/avr-differential/run_avr8js.js build/tools/avr8js/node_modules 73
```
