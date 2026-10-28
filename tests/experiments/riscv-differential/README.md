# RISC-V differential test (E-08)

Compares SimNodus's RV32IMC core (`src/emu/riscv_cpu.*`) with QEMU on random
instruction blocks, for gate G8a.2 of the
[E-08 plan](../../../docs/experiments/E-08-plan.md).

- `generate.py` writes an ELF for QEMU's `virt` board, which SimNodus's RISC-V
  test machine copies (RAM at 0x80000000, 16550 UART at 0x10000000, test
  finisher at 0x100000). Each block loads x1-x31 from a table (special values
  such as 0, -1 and INT_MIN with probability 0.15), runs 16 random
  instructions from every decoder entry that cannot trap, and stores the
  registers and mscratch back. Loads and stores use a 64-byte scratch area
  through x2 and x8, which no instruction writes (the `c.addi16sp` instructions
  come in pairs that restore sp); branches and jumps (`jalr` and `c.jr` with a
  preceding `auipc`) go forward inside the block. At the end the program sends
  every block's table and scratch area to the UART. The same seed gives the
  same ELF.
- QEMU's serial output is the reference (`*.qemu.bin`).
- `corpus/seed1-512.elf` (seed 1, 512 blocks) and QEMU's output run in ctest
  as `emu-riscv-differential`, without QEMU installed; the test also prints
  the decoder entries that the corpus hits fewer than 1 000 times.

```text
python tests/experiments/riscv-differential/generate.py --seed 7 --blocks 512 --out build/riscv-diff/s7.elf
qemu-system-riscv32 -M virt -bios none -kernel build/riscv-diff/s7.elf -display none -monitor none -serial file:build/riscv-diff/s7.qemu.bin -no-reboot
```

QEMU is Espressif's build esp_develop_9.2.2_20250817 (`qemu-system-riscv32`,
the version ESP-IDF v5.5.2 pins), used only as a separate program; it is GPL
and is never linked or shipped. `ecall`, `ebreak`, `c.ebreak`, `mret` and
`wfi` trap or sleep, so the random blocks leave them out; `emu-riscv-unit`
covers them with directed cases. A difference counts as a QEMU deviation only
when a third reference (the specification, the architecture tests or the
owner's board) agrees with the emulator; otherwise it is an emulator bug.
