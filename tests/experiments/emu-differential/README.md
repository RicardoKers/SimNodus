# Emulator differential test (E-07)

Historical: the comparison program (`tests/unit/emu_differential.cpp`) and its
ctest entry were removed with Renode on 2026-10-18 (ADR 0094); the generator
and the corpus stay as the record of gate G0a.2.

Compares the SimNodus STM32 emulator (`src/emu`) with Renode 1.16.1 on random
Thumb-2 instruction blocks, for gate G0a.2 of the
[emulator plan](../../../docs/planning/MCU_EMULATOR_PLAN.md).

- `generate.py` writes a batch ELF: each block sets every register and the
  flags, runs random instructions from every ARMv7-M encoding group (no
  UNPREDICTABLE forms; memory accesses stay in a 64-byte scratch area), then
  stores R0-R12, LR, APSR, the scratch words and PRIMASK (32 words per block).
  The same seed gives the same ELF.
- `diff.repl` is the bare Cortex-M3 that Renode runs (1 MiB flash and SRAM);
  the emulator side builds the same memory map.
- `tests/unit/emu_differential.cpp` runs a batch in both, compares every
  result word and prints the emulator's decoder coverage.
- `corpus/seed1-512.elf` (seed 1, 512 blocks of 16 instructions) runs in ctest
  as `emu-differential` when Renode is installed.

Exceptions, faults and sleep are not random-tested here (a block must not
trap); `emu-unit` covers them with directed cases.

```text
python tests/experiments/emu-differential/generate.py --seed 7 --blocks 512 --out build/emu-diff/s7.elf
build/dev/tests/unit/emu_differential <renode> resources/renode tests/experiments/emu-differential/diff.repl 512 build/emu-diff/s7.elf
```

A difference is a Renode deviation only when a third reference (the ARMv7-M
pseudocode, QEMU or the owner's board) agrees with the emulator; otherwise it
is an emulator bug.
