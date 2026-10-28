# E-08: ESP32-C3 emulator prototype — results

Date: 2026-10-28. Tasks: SN-242 (prerequisites), SN-243 (core). Result so
far: **gate G8a met**; gate G8b (firmware, SN-244 and SN-245) not started.
Plan and predeclared criteria: [E-08-plan.md](E-08-plan.md); they are not
repeated here.

## Decisions

- **DE0 (owner, 2026-10-28): vendor the ROM ELF.** Espressif's
  `esp32c3_rev3_rom.elf` (esp-rom-elfs `20260528`, Apache-2.0) goes into
  `third_party/esp-rom-elfs` with its licence when SN-244 needs it, and is
  checked against a ROM read from the owner's board when the boards arrive.
- The owner approved starting the core (SN-243) before the boards arrive.

## Environment and provenance

Windows 11, MSVC 19.51 (Release, `/O2`), owner's laptop. Linux: GCC 13 in CI.
References: The RISC-V Instruction Set Manual, volume I (20191213) and
volume II (20211203); riscv-arch-test 2.7.4 (commit
`96e3589a867955e94c95ba098822da54d3ae39c0`); Espressif's QEMU
esp_develop_9.2.2_20250817 (`qemu-system-riscv32 -M virt`, the version
ESP-IDF v5.5.2 pins; the plan named esp-develop-9.2.2-20260417, the newest
release, but ESP-IDF's pin is the better reference); Espressif's GCC
esp-14.2.0_20251107 for the test programs and the instruction words of the
directed cases.

## What was built

- `src/emu/riscv_cpu.*`: RV32IMC with Zicsr and Zifencei, machine and user
  modes, 16 PMP entries (OFF, TOR, NA4, NAPOT, locking), traps with mepc,
  mcause and mtval, vectored or direct mtvec, interrupt lines from the
  machine with an optional priority hook (for the ESP32-C3's controller),
  WFI that sleeps until an enabled line rises, mcycle/minstret with
  mcounteren, hooks for vendor CSRs. Predecoded instructions per halfword
  for plain-memory code regions; the core's stores, `fence.i` and the
  machine invalidate them. One instruction per cycle (an approximation,
  labelled). Misaligned loads and stores trap, as the plan assumes for the
  ESP32-C3 (to be checked on the board).
- `src/emu/riscv_test_machine.*`: RAM, a 16550 transmit register, the test
  finisher and a CLINT timer at QEMU `virt`'s addresses, so that one ELF runs
  unchanged in both. Not a model of any chip.
- `src/emu/elf.*`: RISC-V ELF files and named symbols (the STM32 path still
  accepts only ARM files and now says when a file is built for RISC-V).
- `simnodus-emu` runs a RISC-V ELF on the test machine.
- Tests: `emu-riscv-unit` (20 directed cases), `emu-riscv-arch` (90
  architecture tests), `emu-riscv-differential` (the seed-1 corpus against
  QEMU's recorded output).

## Gate G8a — core

| ID | Criterion (short) | Status | Evidence |
|---|---|---|---|
| G8a.1 | riscv-arch-test I, M, C, Zicsr (privilege), Zifencei signatures | **met** | 90 of 90 signatures equal the suite's references: I 38, M 8, C 27, privilege 16, Zifencei 1 (`emu-riscv-arch`). Every test whose `RVTEST_CASE` conditions this core meets was built; none was left out. The misaligned load/store tests ran in their variant for hardware that traps |
| G8a.2 | Random differential, every decoder entry 1 000 hits, zero unexplained differences | **met** | 16 seeds of 512 blocks of 16 instructions (8 192 blocks) against QEMU: 0 differing bytes; every decoder entry that the blocks may contain was hit at least 1 000 times (the summed counters of the 16 runs). `ecall`, `ebreak`, `c.ebreak`, `mret` and `wfi` trap or sleep and are covered by directed cases only, as WFI, SVC and BKPT were in E-07. Seed 1 runs in CI. Sanity check: an `sra` computed as a logical shift was detected by the differential test (39 differing words in seed 1) and by the architecture tests (89 of 90) |
| G8a.3 | ≥ 150 MIPS busy loop, ≥ 100 MIPS mix | **met on Windows**; Linux on CI | Owner's laptop, Windows (MSVC): busy loop 164-165 MIPS (1.03x real time at 160 MHz), benchmark mix 125-128 MIPS (0.8x), each over 3 s of firmware time, three runs. The mix's checksums equal QEMU's. CI prints both on Linux and Windows (step "Emulator speed") |
| G8a.4 | Decoding never crashes; random code never hangs | **met** | All 65 536 16-bit patterns and 1 000 000 random 32-bit patterns decode to a valid entry of the right size; 200 random 1 KiB images run 5 000 cycles each; a trap vector with no code halts the core with a message while time goes on (`emu-riscv-unit`) |

## Observations

- Three corrections came from the references, not from the core. The
  architecture tests need the `RVTEST_ISA` of each test as `-march` (compiled
  with C, `jal-01`'s signature changes because the assembler compresses
  instructions) and the `RVTEST_CASE` definitions per test (a global
  `rvtest_mtrap_routine` adds a trap signature area that the references do
  not have). Spike's `model_test.h` puts `tohost` and 256-byte-aligned
  register state before the signature, which moves the addresses that the
  misaligned-access tests record; ours puts nothing there.
- The directed cases found two errors in their own first version (an
  instruction word and a missing return to machine mode), none in the core.

## Next

SN-244: the ESP32-C3 SoC and boot path (ROM ELF into `third_party`, flash
images, cache and MMU, interrupt matrix, SYSTIMER, watchdogs, eFuse, UART,
USB-Serial-JTAG, GPIO) toward criteria G8b.1-3 and G8b.5-9. The owner's
boards (DevKitM-1, SuperMini) are needed for G8b.1's byte-for-byte console
comparison and the ROM check.
