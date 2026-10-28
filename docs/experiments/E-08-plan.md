# E-08: ESP32-C3 emulator prototype — plan

Date: 2026-10-28. Tasks: SN-242..SN-245 in the [backlog](../planning/BACKLOG.md).
Status: **planned**; no code yet. The criteria below are recorded before any
emulator code, as for [E-07](E-07-results.md), and are not relaxed after a run:
a failed criterion leads to a recorded decision.

## Question

Can SimNodus's own emulator run **unmodified** ESP32-C3 firmware (the flash
images that ESP-IDF, Arduino-ESP32 and the official MicroPython release
produce) from the chip's reset through the ROM and the second-stage
bootloader, up to the MicroPython REPL on the serial console, with the
flash contents kept between runs, fast enough to be usable, and without
changing anything that the STM32 path does today?

## Why the ESP32-C3 first

Owner's decision of 2026-10-28, after the feasibility analysis of the same
day: the lessons can be compiled for the ESP32 DevKit V1 (Xtensa LX6) or the
ESP32-C3, so the chip that suits the simulator comes first.

- One RISC-V core (RV32IMC, machine and user modes) instead of two Xtensa
  LX6 cores with register windows, zero-overhead loops, MAC16 and an FPU.
  The ISA and its test suites are open.
- 160 MHz on one core: about 160 million instructions per second for real
  time, within reach of the present interpreter design (120-185 MIPS on the
  Thumb-2 core, E-07). The ESP32 would need about 480 million.
- Fewer and more regular peripherals. The ESP-IDF boot path (ROM, flash
  cache, eFuse, clock calibration, watchdogs) is as hard as on the ESP32,
  but without the second core.
- What the C3 lacks for lessons: DAC, touch and Hall sensors; ADC2 is not
  reliable on the chip (errata), so analog inputs use ADC1. A DAC lesson
  becomes PWM with an RC filter, which the circuit simulates.
- Wi-Fi (phase 2, experiment E-09) has less public reverse engineering for
  the C3 than for the ESP32 (esp32-open-mac). This is accepted; it does not
  affect phase 1.

The ESP32 (Xtensa) stays possible later (SN-251): the integration work
(SN-246) makes the runner independent of the chip.

## Scope

**In:** the RV32IMC core with Zicsr, machine and user modes and the PMP the
ESP-IDF uses; the C3's interrupt controller and interrupt matrix; the ROM;
the external flash with its cache and MMU; the SoC registers the ESP-IDF
touches while booting; UART0/1, USB-Serial-JTAG, GPIO; a command-line runner
(`simnodus-emu --machine esp32c3`), as E-07 started.

**Out (later milestones, SN-246..SN-250):** integration in the session and the
GUI, LEDC, ADC, I2C, SPI, RMT, GDMA, GDB, Wi-Fi, Bluetooth, deep sleep,
cryptographic accelerators beyond what the boot path needs.

## Prerequisites (before emulator code)

1. **ROM source and licence (SN-242, decision DE0).** The firmware calls ROM
   functions directly, so the 384 KB mask ROM is needed. Options, in order of
   preference:
   1. ship the ROM image Espressif distributes (the `esp-rom-elfs` releases,
      the ROM files of Espressif's QEMU) if its licence allows
      redistribution in an MIT application; ESP-IDF's copyright page lists
      only BSD, MIT and public-domain code inside the ROM, which is
      encouraging but not enough;
   2. a ROM image the user reads from a board with `esptool` and points
      SimNodus to once (never downloaded while opening a project);
   3. our own implementation of the ROM functions the ESP-IDF calls
      (high-level emulation), only if 1 and 2 fail.
   The owner decides with the licence text in hand.
2. **Pinned references.** ESP-IDF version (the one the pinned MicroPython
   release is built with), Arduino-ESP32 3.x, the official MicroPython
   `ESP32_GENERIC_C3` release, the chip revision the eFuse model reports and
   its ROM version. Every image used is identified by SHA-256 in the report.
3. **Oracles.** Espressif's QEMU (`qemu-system-riscv32 -M esp32c3`) as a
   separate, test-only process, like Renode and QEMU in E-07; it is GPL and
   is never linked or copied. A real ESP32-C3-DevKitM-1 (UART0 through a USB
   bridge) and an ESP32-C3 SuperMini (USB-Serial-JTAG only), bought by the
   owner, are the final reference. The RISC-V specification and the
   `riscv-arch-test` suite are the references for the core.
4. **Clean-room rule.** Behaviour comes from the Technical Reference Manual,
   the ESP-IDF sources (Apache-2.0: register headers and HAL may be read and,
   with a notice, reused), the oracles' observed behaviour and the board.
   Code from GPL or AGPL emulators (Espressif's QEMU, Velxio) is not read
   while writing the corresponding model and is never copied.

## Work packages

| # | Package | Done when |
|---|---|---|
| 0.1 | `src/emu/riscv` core: RV32I, M, C, Zicsr, machine and user modes, traps, `mret`, `wfi` (time skips to the next event), PMP; predecoded instructions and the dispatch of E-07; decoder hit counters | Instruction tests pass |
| 0.2 | `riscv-arch-test` (rv32i_m I, M, C, Zicsr, privilege) in ctest; random differential blocks against QEMU, batched like `emu-differential` | Results recorded |
| 0.3 | Benchmark firmware (busy loop, CoreMark-like mix) built with the ESP-IDF toolchain | Numbers recorded → **G8a** |
| 0.4 | Flash images: a merged `.bin`, or bootloader, partition table and application at their offsets; parse the image headers for diagnostics; an optional application ELF for function names in diagnostics; the flash contents saved to a file and loaded on the next run | Every test image loads; the CLI lists partitions and segments |
| 0.5 | SoC tier 1: ROM mapping; SPI0/SPI1 flash controller and a 4 MB flash chip (JEDEC ID, status, read modes, erase, program); cache and MMU (64 KB pages); interrupt matrix and the CPU interrupt controller (priorities, threshold); SYSTIMER; TIMG0/1 with their watchdogs and the RTC slow-clock calibration; RTC_CNTL (reset reasons, RTC watchdog, super watchdog, brownout); SYSTEM and clock registers; eFuse (MAC, chip revision, ADC calibration block); RNG; assist-debug stack guard and the permission (memprot) registers the ESP-IDF enables by default; strapping pins GPIO2/8/9 read at reset | ESP-IDF `hello_world` prints to the console |
| 0.6 | UART0/1, USB-Serial-JTAG, GPIO with IO_MUX and the GPIO matrix; unknown registers read their reset value and every access to an unmodelled peripheral is a diagnostic naming it | The firmware set of G8b runs in the CLI |
| 0.7 | Trace comparison (console bytes, GPIO events with times) against QEMU and the boards | Results recorded |
| 0.8 | MicroPython: REPL, files, reset kinds, faults | Results recorded → **G8b** |
| 0.9 | Report `docs/experiments/E-08-results.md` | Gates reviewed with the owner → decision **DE1** |

## Gate G8a — core

| ID | Criterion |
|---|---|
| G8a.1 | `riscv-arch-test` for rv32i_m I, M, C, Zicsr and privilege: every test's signature matches the reference |
| G8a.2 | Random differential against QEMU: every decoder entry hit at least 1 000 times, with operands that include x0 and the stack pointer, shifts by 0 and 31, and compressed and full encodings mixed; zero unexplained differences. A difference counts as a QEMU deviation only when a third reference (the specification, the arch-test reference or the real board) agrees with our result |
| G8a.3 | At least 150 MIPS on the busy loop and 100 MIPS on the benchmark mix, Release build, owner's laptop on Windows (MSVC); Linux (GCC) recorded on CI. With one instruction per cycle this is about 0.9x real time at 160 MHz for firmware that never waits; `wfi` lets waiting firmware run faster than real time |
| G8a.4 | Every 16-bit pattern and 1 000 000 random 32-bit patterns decode without crashing the host; 200 random code images run 5 000 cycles each without a crash or hang |

## Gate G8b — firmware

| ID | Criterion |
|---|---|
| G8b.1 | Unmodified merged images of ESP-IDF `hello_world`, `blink` (GPIO), an `esp_timer` example and a FreeRTOS example with two tasks and a queue boot from the chip's reset through the ROM and the second-stage bootloader. Their console output equals the board's byte for byte, except lines listed in the report that carry measured times or the MAC address |
| G8b.2 | The `blink` period equals the programmed period within 0.1 % (the FreeRTOS tick comes from SYSTIMER). Instruction timing is one instruction per cycle and is labelled approximate: flash cache misses are not modelled |
| G8b.3 | An Arduino-ESP32 sketch with `Serial` and a GPIO output runs unchanged from the IDE's merged `.bin` |
| G8b.4 | The official MicroPython `ESP32_GENERIC_C3` release image: the REPL prompt appears on USB-Serial-JTAG; `machine.Pin` toggles a GPIO from the REPL; `time.sleep_ms(500)` lasts 500 ms of virtual time within 0.1 %; a file written with `open()` is there after the emulator restarts from the saved flash file; a `main.py` placed in the filesystem image runs at boot |
| G8b.5 | Start-up on the owner's laptop: from loading the image to the ESP-IDF `app_main` under 1 s, to the MicroPython prompt under 3 s wall-clock time; the first boot, when MicroPython creates its filesystem, is measured and reported separately |
| G8b.6 | Faults: an illegal instruction, a load from an unmapped address and an ESP-IDF `abort()` print the ESP-IDF panic report on the console, as on the board, and give an emulator diagnostic with the PC (and the function name when an ELF is given); a watchdog timeout resets the chip as on the board; the host never hangs |
| G8b.7 | Resets: `esp_restart()`, the RTC and TIMG watchdogs and the reset button give the reset reason the ROM prints on the board (for example `rst:0xc (RTC_SW_CPU_RST)`) |
| G8b.8 | Two runs of the same image with the same scripted inputs give identical traces (hash) |
| G8b.9 | Nothing changes for the STM32: every existing ctest suite and `tools/examples_pass.py` pass unchanged; the STM32 MIPS of `emu-benchmark` stay within 3 %; the ESP32-C3 code lives in its own files; no third-party code beyond permissive licences (littlefs, BSD-3, only if a host-side filesystem tool is needed) |

## Outcomes (decision DE1)

- G8a and G8b pass: an ADR for the ESP32-C3 on the own emulator, then
  integration (SN-246) and the lesson peripherals (SN-247, SN-248).
- Only G8a.3 (speed) fails: one more week for performance, then re-review.
- A boot-path criterion fails twice, or the ROM cannot be obtained under
  acceptable terms (DE0): the experiment stops with its measured reasons, and
  the owner decides whether to wait or to try the ESP32 instead.

## Later milestones (tentative, detailed only after DE1)

- **Integration (SN-246):** an `emu::Machine` interface so that
  `NativeMcuRunner` no longer creates `Stm32F103` itself; pin state and the
  electrical pin model generalized (drive strengths, pull-up and pull-down,
  input enable); board profiles ESP32-C3-DevKitM-1 and ESP32-C3 SuperMini;
  flash images and the flash file in projects and in File > Save with
  Firmware; strapping pins read from the circuit at reset; the terminal on
  UART0 or USB-Serial-JTAG.
- **Lesson peripherals (SN-247):** LEDC, ADC1 (one-shot and continuous with
  GDMA), I2C0, SPI2 with GDMA, RMT (WS2812), temperature sensor.
- **MicroPython workflow (SN-248):** `main.py` and other files from the project
  into the flash filesystem; the console reachable from Thonny and `mpremote`
  (a TCP serial URL or a virtual port pair, to be verified); user guide in
  English and pt-BR.
- **Debugging (SN-249):** the GDB server for RISC-V.
- **Wi-Fi (SN-250, experiment E-09, its own plan before any code):** a model of
  the Wi-Fi MAC at the register and DMA-descriptor level so that the
  unmodified Wi-Fi libraries run (function interception is rejected: release
  images have no symbols and the libraries' internals change between ESP-IDF
  versions); a virtual access point with a configurable SSID, open first and
  WPA2-PSK next; DHCP and DNS; a user-space NAT gateway to the computer's
  network that terminates TCP and UDP on operating-system sockets, enabled
  explicitly per project; port forwarding from the computer to a board's web
  server; several boards on one access point.

## Risks

1. ROM licence (DE0); the fallback options cost user friction or a large
   high-level emulation.
2. Boot-path details that hang silently (polling loops on calibration and
   status registers); mitigated by diagnostics for every unmodelled access
   and by the oracles.
3. The permission (memprot) and stack-guard hardware the ESP-IDF enables on
   the C3 by default.
4. Instruction timing is approximate (no cache-miss model); labelled as such.
5. Wi-Fi depends on reverse engineering undocumented hardware (E-09).

## References

- ESP32-C3 Technical Reference Manual and datasheet (Espressif).
- ESP-IDF sources (Apache-2.0): `components/soc/esp32c3`, `components/hal`,
  `components/esp_rom`, `components/bootloader_support`.
- The RISC-V Instruction Set Manual, volumes I and II; `riscv-arch-test`.
- Espressif's QEMU (test-only oracle):
  https://github.com/espressif/qemu
- Wi-Fi reverse engineering for E-09: https://github.com/esp32-open-mac/qemu
