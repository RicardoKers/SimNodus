# RISC-V benchmarks (E-08)

Speed of the RV32IMC core for gate G8a.3 of the
[E-08 plan](../../../docs/experiments/E-08-plan.md) (at least 150 MIPS on a busy
loop and 100 MIPS on the benchmark mix).

- `bench.c`: the integer mix of the STM32 benchmark
  (`tests/experiments/emu-benchmark`: CRC32, memory copies, insertion sort,
  division, recursion) with a 1 kHz CLINT timer interrupt, never sleeping; a
  checksum per round on the UART, equal to QEMU's.
- `busy.S`: a counter and a branch, as firmware that polls a flag.

Both run on SimNodus's RISC-V test machine (160 MHz, the ESP32-C3's clock)
and on QEMU's `virt` board. The ELF files are committed so that CI prints the
speed on Linux and Windows (step "Emulator speed").

```text
python tests/experiments/riscv-benchmark/build.py build/tools/riscv32-esp-elf/bin/riscv32-esp-elf-gcc
build/dev/apps/simnodus-emu/simnodus-emu tests/experiments/riscv-benchmark/busy.elf --ms 3000 --quiet
build/dev/apps/simnodus-emu/simnodus-emu tests/experiments/riscv-benchmark/bench.elf --ms 3000 --quiet
```

Measured on 2026-10-28 on the owner's laptop (Windows, MSVC Release): busy
loop 164-165 MIPS (1.03x real time at 160 MHz), mix 125-128 MIPS (0.8x).
