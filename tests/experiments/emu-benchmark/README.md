# Emulator benchmark mix (E-07)

`bench.c` is integer work that resembles firmware (CRC32, memory copies,
insertion sort, division, recursion) at 72 MHz with a 1 kHz SysTick interrupt
and no sleeping, for gate G0a.3 of the
[emulator plan](../../../docs/planning/MCU_EMULATOR_PLAN.md) (at least 100 MIPS).
It prints a checksum per round on USART1, so two emulators can be compared.

```text
python tests/experiments/emu-benchmark/build.py
build/dev/apps/simnodus-emu/simnodus-emu tests/experiments/emu-benchmark/bench.elf --ms 3000 --quiet
```

The ELF is built locally (not committed). Measured on 2026-10-15 on the owner's
laptop (Windows, MSVC Release): 119-141 MIPS.
