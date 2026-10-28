# E-08: ESP32-C3 emulator prototype — results

Date: 2026-10-28. Tasks: SN-242 (prerequisites), SN-243 (core), SN-244 (SoC
and boot, in progress). Result so far: **gate G8a met**; gate G8b partly
measured (table below).
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
| G8a.3 | ≥ 150 MIPS busy loop, ≥ 100 MIPS mix | **met on Windows**; Linux on CI | Owner's laptop, Windows (MSVC): busy loop 164-165 MIPS (1.03x real time at 160 MHz), benchmark mix 125-128 MIPS (0.8x), each over 3 s of firmware time, three runs. The mix's checksums equal QEMU's. GitHub CI runners (slower, shared; PR #238): Linux (GCC) 118.9 and 84.2 MIPS, Windows (MSVC) 51.5 and 68.7 MIPS; the STM32 benchmark in the same run gave 58.9 (Linux) and 50.2 (Windows) MIPS. The Linux criterion is measured on CI only, as in E-07, because the owner has no Linux machine; the Windows runner's busy loop below its mix is runner noise to watch in later runs |
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

## Gate G8b — firmware (in progress)

SN-244, first part (2026-10-28): `src/emu/esp32c3.*` with the memory map
(ROM and its data-bus view, SRAM0, SRAM1 through both buses, RTC FAST memory,
the 8 MiB flash window through the cache), the ROM rebuilt from the vendored
ELF (`src/emu/esp32c3_rom.*`), and the peripherals the boot path needs:
SPI0/SPI1 with a GD25Q-like flash chip (JEDEC C8 40 16, read, program, erase,
status), EXTMEM and the 128-entry MMU, eFuse (chip v0.4, eFuse block v1.3, a
MAC address), SYSTEM (CPU clock selection, FROM_CPU interrupts), SYSCON's RNG
(deterministic), TIMG's RTC slow clock calibration, SYSTIMER (two units, three
comparators), the interrupt matrix and controller, SHA-1/224/256 in CPU mode
(`src/emu/sha.*`), UART0/1 transmit, USB-Serial-JTAG in both directions with
its interrupt, CPU and system resets with their reasons, and a register block
that keeps values for every other peripheral page. Vendor CSRs: mpcer, mpcmr,
mpccr and the undocumented 0x800-0x802 that the ROM uses (0x802 counts cycles
for `ets_delay_us`). `simnodus-emu --esp32c3` runs a flash image (`--usb` and
`--uart0` type text, `--trace-io` traces register accesses, `--flash-out`
saves the flash). Tests: `emu-esp32c3`.

| ID | Criterion (short) | Status | Evidence |
|---|---|---|---|
| G8b.1 | ESP-IDF images boot unmodified; console equal to the board's | **boots**; board comparison waits for the boards | `hello_world` (ESP-IDF v5.5.2): ROM banner `ESP-ROM:esp32c3-api1-20210207`, `rst:0x1 (POWERON),boot:0xc (SPI_FAST_FLASH_BOOT)`, the ROM loading and checking the bootloader (SHA-256 verified by the SHA model), the second-stage bootloader (chip v0.4, partition table, six application segments), `cpu_start`, `heap_init`, `spi_flash: detected chip: gd`, `Hello world!` and `This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision v0.4, 2MB external flash`. Known difference to check on the board: after a CPU reset the chip also prints `Saved PC:`. The blink, esp_timer and FreeRTOS examples are still to run |
| G8b.2 | `blink` period within 0.1 % | **met for the FreeRTOS tick** | `hello_world`'s countdown (`vTaskDelay(1000 ms)`): lines exactly 1000.000 ms apart (1028.2037, 2028.2037 ms, ...). GPIO toggling (`blink`) comes with the GPIO model |
| G8b.3 | Arduino-ESP32 image runs unchanged | not yet | — |
| G8b.4 | MicroPython REPL, Pin, sleep_ms, files across restarts, `main.py` at boot | **REPL and files met**; Pin and sleep_ms with the GPIO model | Official image `ESP32_GENERIC_C3-20260824-v1.29.0.bin`: `Performing initial setup` (the file system created through SPI1 erase and program), `MicroPython v1.29.0 on 2026-08-24; ESP32C3 module with ESP32-C3`, `>>>` on USB-Serial-JTAG; `print(6*7)` gives `42`; a `main.py` written from the REPL runs at the next boot from the saved flash, without the initial setup |
| G8b.5 | `app_main` under 1 s, MicroPython prompt under 3 s | **met** | Whole `simnodus-emu` process on the owner's laptop, ROM rebuild included: `hello_world` to `app_main` (29 ms of firmware time) 0.052-0.054 s; MicroPython to `>>>` on first boot (152 ms) 0.215-0.217 s, from a saved flash 0.17 s. Firmware that waits in WFI runs much faster than real time (12 s of `hello_world` in 0.37 s) |
| G8b.6 | Faults: panic report, diagnostic with PC, watchdog reset | partly | An access to an unmapped peripheral (before every page was covered) gave the ESP-IDF panic report (`Guru Meditation Error: Core 0 panic'ed (Load access fault)`, register dump, `Rebooting...`) and the emulator's diagnostic with the PC and function (`rtc_sleep_pu`). Watchdog resets not modelled yet |
| G8b.7 | Reset reasons as the ROM prints them | **CPU reset met**; watchdogs and the reset button to come | `esp_restart()`: `rst:0xc (RTC_SW_CPU_RST),boot:0xc (SPI_FAST_FLASH_BOOT)` and a second boot |
| G8b.8 | Identical traces on two runs | **met** | Two runs of `hello_world` for 1.2 s: identical output bytes and times (`emu-esp32c3`); the RNG is a fixed-seed generator for this reason |
| G8b.9 | Nothing changes for the STM32 | **met so far** | `emu-unit` 51/51 and every ctest suite pass; the STM32 code is unchanged except the bus's fast-memory table (4 to 8 entries, checked in order, so the STM32's three memories behave as before) and the ELF reader (also sections) |

Observations:

- The ROM ELF lacks part of the ROM in its program headers; rebuilding it
  from the sections and the ROM's own `.data` copy table gives an image equal
  byte for byte to a ROM read from a chip (SHA-256
  `0de1e65020e803bea0d7443dca149d61895e01fca3bb9c82d073234eebd73f99`,
  compared locally with the ROM file that Espressif ships with its QEMU,
  which is not redistributed). The owner's board will be the second check.
- The interrupt controller takes a line whose priority is at least the
  threshold (ESP-IDF runs with threshold 1 and priority-1 interrupts); "above
  the threshold" kept FreeRTOS from starting.
- The decoded-instruction cache of the core is now paged (4 KiB pages made
  on first use, for the 8 MiB flash window) with the last page remembered:
  the busy loop went from 164 to 183-185 MIPS, the mix stayed at 130-137.

## Next

SN-244, rest: GPIO (outputs, inputs, the GPIO matrix and IO_MUX, pin events
with times), UART receive, the watchdogs (TIMG MWDT, RTC WDT, super
watchdog), the blink, esp_timer and FreeRTOS examples and the Arduino-ESP32
image; then SN-245. The owner's boards (DevKitM-1, SuperMini) are needed for
G8b.1's byte-for-byte console comparison and the ROM check.
