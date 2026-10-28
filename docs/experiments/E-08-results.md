# E-08: ESP32-C3 emulator prototype — results

Date: 2026-10-28. Tasks: SN-242 (prerequisites), SN-243 (core), SN-244 (SoC
and boot), SN-245 (MicroPython). Result so far: **gate G8a met**; gate G8b met
except the console comparison with the owner's board (G8b.1).
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

SN-244, second part (2026-10-28): GPIO (pads, the GPIO matrix's simple output,
IO_MUX function and pulls, inputs from the circuit, edge and level interrupts,
pin events with times), UART receive (RX FIFO, full and time-out interrupts),
the TIMG and RTC watchdogs (four stages: interrupt, CPU, system or RTC reset,
with their reset reasons and a diagnostic when one runs out), light and deep
sleep with RTC-timer wake-up (the digital timers stand still; a deep sleep
wakes through a reset with the RTC domain kept), the reset button, and
misaligned loads and stores done by the core (see the observations). CLI:
`--uart1`, `--input GPIO=level@ms`, pin events in the output.

| ID | Criterion (short) | Status | Evidence |
|---|---|---|---|
| G8b.1 | ESP-IDF images boot unmodified; console equal to the board's | **boot met**; board comparison waits for the boards | `hello_world`, `blink`, `esp_timer` and `real_time_stats` (ESP-IDF v5.5.2, unmodified examples) run from the ROM through the bootloader: the ROM banner, `rst:0x1 (POWERON),boot:0xc (SPI_FAST_FLASH_BOOT)`, the bootloader (chip v0.4, partition table), `spi_flash: detected chip: gd`, the applications' own output. The byte-for-byte comparison with the board (timing values and the MAC excepted) needs the owner's boards; known difference to check: after a CPU reset the chip also prints `Saved PC:` |
| G8b.2 | `blink` period within 0.1 % | **met** | ESP-IDF `blink` on GPIO8 at 500 ms: edges 500.002, 499.999, 500.000 ms apart (< 0.001 %); `hello_world`'s `vTaskDelay(1000 ms)` 1000.000 ms; `esp_timer`'s periodic timer 500 000 µs |
| G8b.3 | Arduino-ESP32 image runs unchanged | **met** | `arduino_serial_blink.ino` (Arduino-ESP32 3.3.12, ESP32C3 Dev Module): `Serial` output, `millis()`, `delay(250)` toggling GPIO8 every 250.0 ms, characters received on UART0 echoed in capitals |
| G8b.4 | MicroPython REPL, Pin, sleep_ms, files across restarts, `main.py` at boot | **met** | Official `ESP32_GENERIC_C3-20260824-v1.29.0.bin`: `>>>` on USB-Serial-JTAG; `print(6*7)` gives `42`; `machine.Pin(5, Pin.OUT)` toggled with `time.sleep_ms(500)`: edges 500.004 ms apart (< 0.001 %); an input with `PULL_UP` reads 1; `Pin.irq(..., IRQ_FALLING)` counts three falling edges set from outside; `machine.UART(1)` reads bytes sent from outside and writes bytes out; `main.py` written from the REPL runs at the next boot from the saved flash; `machine.deepsleep(1000)` wakes 1 s later with `rst:0x5 (DSLEEP)`, `reset_cause()` 4 and `wake_reason()` 4 |
| G8b.5 | `app_main` under 1 s, MicroPython prompt under 3 s | **met** | Whole `simnodus-emu` process on the owner's laptop, ROM rebuild included: `hello_world` to `app_main` 0.052-0.054 s; MicroPython to `>>>` on first boot 0.215-0.217 s, from a saved flash 0.17 s |
| G8b.6 | Faults: panic report, diagnostic with PC, watchdog reset | **met** | `faults` (our ESP-IDF application): an illegal instruction, a load from address 0x10 and `abort()` each give ESP-IDF's panic report (`panic'ed (Illegal instruction)`, `panic'ed (Load access fault)`, `abort() was called at PC ...`), the emulator's diagnostic with the PC and function (`Exception 2 at PC 0x42008CC4 (in app_main)`), and a restart. MicroPython's unfed `machine.WDT` and an endless loop with interrupts off give `Task watchdog got triggered` and `panic'ed (Interrupt wdt timeout on CPU0)` |
| G8b.7 | Reset reasons as the ROM prints them | **met** (CPU, system watchdog, deep sleep, reset button) | `esp_restart()`: `rst:0xc (RTC_SW_CPU_RST)`; the RTC watchdog with stage 0 = system reset: `rst:0x9 (RTCWDT_SYS_RST)`; deep sleep: `rst:0x5 (DSLEEP)`; the reset button: `rst:0x1 (POWERON)`. The TIMG watchdogs' reset reasons (0x7, 0x8, 0xB, 0x11) use the same code path, not exercised by a firmware yet |
| G8b.8 | Identical traces on two runs | **met** | `emu-esp32c3`: two runs of `hello_world` give identical output bytes and times |
| G8b.9 | Nothing changes for the STM32 | **met** | Every ctest suite passes; the STM32 code is unchanged except the bus's fast-memory table and the ELF reader |

Observations:

- The ESP32-C3 core performs misaligned loads and stores: Arduino-ESP32's
  newlib `strcmp` (Espressif toolchain) loads words at odd addresses and the
  sketch runs on the chip; with a trap, it panicked (`Load address
  misaligned`). The machine now lets the core do them byte by byte; the test
  machine still traps, as the architecture tests expect. To confirm on the
  board, with the cycle cost.
- Light sleep: ESP-IDF's `esp_timer` example asserts that 0.5 s of light
  sleep measures 0.5 s by `esp_timer` (499 682 µs here): SYSTIMER stands
  still and ESP-IDF adds the RTC timer's count. The log's millisecond stamps
  stop during the sleep (FreeRTOS ticks); to compare with the board.
- Not modelled yet: wake-up from GPIO or UART, the RTC memory CRC before deep
  sleep (it finishes without a value, so the ROM never runs a deep-sleep wake
  stub), the permission (memprot) registers (kept, not enforced), the super
  watchdog (taken as fed), UART baud-rate timing (bytes leave at once), the
  TIMG general-purpose timers.
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

## Lesson peripherals (SN-247, started 2026-10-28, ahead of DE1)

The owner has no board for G8b.1 yet and asked to go on with what does not
need one. Done at the machine level (no integration yet):

- LEDC (PWM): four timers (resolution, 10.8 divider, APB/RC_FAST/XTAL clock),
  six channels with HPOINT and DUTY (four fraction bits), applied on
  PARA_UP/DUTY_START, driving GPIO matrix signals 45-50; edges at their exact
  times on the pad. MicroPython `PWM(Pin(5), freq=1000, duty_u16=16384)`:
  period 1.000 ms, high 0.250 ms; `duty_u16(49152)` 0.750 ms; `freq(2000)`
  0.500 ms. Fades go straight to their start duty (not modelled).
- ADC1/ADC2 one-shot conversions (APB_SARADC ONETIME_SAMPLE): linear, the
  inverse of the first step of ESP-IDF's calibration with this machine's
  eFuse, so calibrated readings give the pad's voltage (MicroPython
  `read_uv`: 1.25 V at 12 dB reads 1.249 V, 0.30 V at 0 dB 0.299 V; 2.4 V reads
  2.359 V through the calibration's second step); saturation at 819, 1126,
  1536 and 2805 mV. Labelled approximate (no offset, noise or curve);
  continuous (DMA) conversions not modelled. `set_pin_voltage()`; CLI
  `--input GPIO=1.2V@ms`.
- I2C0 master (after SN-246): the command list (RSTART, WRITE, READ, STOP,
  END), 32-byte FIFOs, interrupts, timing from CLK_CONF and the period
  registers, clock stretching and the timeout, on GPIO matrix signals 53/54,
  with input signals routed by `FUNCn_IN_SEL_CFG`. With SimNodus's LM75 and
  24C02 on `DeviceBus`: MicroPython's `I2C.scan()` gives `[72, 80]`, reads
  and writes match, a missing address gives `ENODEV`; SCL at 100 kHz
  measures 10.00 µs; ESP-IDF's `i2c_master` driver (example 46) and
  Arduino-ESP32's `Wire` (write, read back, NACK code 2 for an empty
  address) work unchanged.
- SPI2 master and GDMA: transactions of CS setup, command, address, dummy
  and data (full or half duplex) and CS hold in modes 0-3, CS0-CS5 with their
  polarity, the clock from CLOCK, bit order from CTRL; data from W0-W15 or
  through GDMA descriptors (three channels, peripheral select, EOF and done
  interrupts); pins through the matrix or IO_MUX function 2. With a W25Q32
  on `DeviceBus`, MicroPython's `machine.SPI` (DMA) reads the JEDEC ID
  `ef 40 16`, programs and reads a page, and gets the status register in
  full duplex; SCK at 1 MHz measures 1.000 µs. ESP-IDF's SPI master driver
  with DMA and two hardware CS lines (example 47: the MCP3208 gives 2047 at
  half of 3.3 V) and Arduino-ESP32's `SPI` (W buffer, no DMA) work unchanged.
- RMT transmit channels 0 and 1: symbols from the RMT RAM (MEM_SIZE
  blocks), the tick from SYS_CONF and DIV_CNT, MEM_TX_WRAP_EN with
  TX_THR_EVENT every TX_LIM words (ESP-IDF's ping-pong refill), the end
  marker with TX_END, loops with TX_LOOP_NUM, the idle level, on signals
  51/52. MicroPython's `neopixel` (192 bits, more than the channel's 48
  words) lights SimNodus's WS2812B with 1 bits high for 800 ns and 0 bits
  for 400 ns; ESP-IDF's RMT driver with a bytes encoder (example 48) and
  Arduino-ESP32's `rgbLedWrite` work unchanged. Not modelled: receive
  channels, the carrier.

## Decision DE1 (2026-10-28, provisional)

The owner took DE1 without waiting for the boards: the ESP32-C3 goes into
the application on the own emulator, marked experimental until G8b.1 is
met ([ADR 0106](../decisions/0106-esp32c3-on-the-own-emulator.md)). The
integration (SN-246) put the runner and `DeviceBus` behind a machine
interface; the ESP32-C3-DevKitM-1 part runs ESP-IDF's blink in an
interactive run (example 45: GPIO8 toggles every 500 ms, 3.17 V high
through the LED, the log on UART0).

## Next

Every G8b criterion is met except G8b.1's comparison with the board. With the
owner's boards (DevKitM-1, SuperMini): read the ROM (`esptool dump-mem`) and
compare it with the rebuilt one, run the same images and compare the
consoles, check misaligned accesses and the `Saved PC:` line, read the chip
revision. Then the report's conclusion and decision DE1 (SN-245).
