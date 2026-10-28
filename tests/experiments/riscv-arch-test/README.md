# RISC-V architecture tests (E-08)

The rv32i_m suites of [riscv-arch-test](https://github.com/riscv-non-isa/riscv-arch-test)
2.7.4 (commit `96e3589a867955e94c95ba098822da54d3ae39c0`), built for SimNodus's
RISC-V test machine, for gate G8a.1 of the
[E-08 plan](../../../docs/experiments/E-08-plan.md). Each test writes a
signature; the reference signatures come with the suite (recorded by the
RISC-V reference models, not by SimNodus).

- `model/model_test.h` and `model/link.ld`: our target files. The signature
  lies between `begin_signature` and `end_signature`; the test ends by writing
  the test finisher at 0x100000.
- `build.py` compiles every test of the I, M, C, privilege and Zifencei suites
  whose `RVTEST_CASE` conditions an RV32IMC_Zicsr_Zifencei core meets when it
  traps on misaligned loads and stores. Each test is compiled for the ISA its
  `RVTEST_ISA` line names, because the references depend on whether the
  assembler may use compressed encodings. GNU as 14 rejects `la x0, label`,
  which `jalr-01` uses; the script replaces it with `auipc x0, 0; addi x0, x0, 0`
  in the preprocessed source, the same size and no effect.
- `corpus/`: the 90 ELF files and their reference signatures. The test sources
  are Copyright (c) 2020 RISC-V International, BSD-3-Clause (their SPDX
  headers; licence text in `corpus/COPYING.BSD`); the ELF files are built from
  them unchanged except for the `la x0` line above.
- ctest `emu-riscv-arch` runs every ELF in the corpus and compares the
  signatures.

```text
git clone --depth 1 --branch 2.7.4 https://github.com/riscv-non-isa/riscv-arch-test.git build/riscv-arch-test
python tests/experiments/riscv-arch-test/build.py --gcc build/tools/riscv32-esp-elf/bin/riscv32-esp-elf-gcc
```

The compiler is Espressif's `riscv32-esp-elf` GCC esp-14.2.0_20251107, the one
ESP-IDF v5.5.2 uses (its `tools/tools.json`).
