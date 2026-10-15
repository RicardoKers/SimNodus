# E-07: SimNodus STM32 emulator prototype

Date: 2026-10-15 (in progress; gate reviews G0a on 2026-10-24 and G0b on
2026-10-30). Task: SN-170. Result: in progress.

## Question and predeclared acceptance criteria

Can an in-process emulator written for SimNodus run the STM32F103 firmware as
correctly as Renode, fast enough for 72 MHz in real time and with a near-instant
start? The criteria were recorded on 2026-10-15 before any emulator code, in the
tables "Gate G0a" and "Gate G0b" of [MCU_EMULATOR_PLAN.md](../planning/MCU_EMULATOR_PLAN.md);
they are not repeated here. Design: [MCU_EMULATOR.md](../architecture/MCU_EMULATOR.md);
decision: [ADR 0089](../decisions/0089-own-stm32-emulator.md) (proposed).

## Environment and provenance

Windows 11, MSVC 19.51 (Release, `/O2`), owner's laptop; Renode 1.16.1 (pinned,
ADR 0082) as oracle; GNU Tools for STM32 14.3.rel1 for test firmware;
STM32CubeF1 1.8.7 for the HAL example. Linux: GCC 13 in CI (build and unit
tests).

## Status of the criteria

| ID | Criterion (short) | Status | Evidence |
|---|---|---|---|
| G0a.1 | Instruction tests from the ARMv7-M pseudocode | passing, to extend | `emu-unit`: flags, shifts by 0/32, multiply/divide, saturation, bitfields, every addressing mode, IT blocks, branches, special registers, exclusive monitor, SVC entry, faults, lockup, SysTick |
| G0a.2 | Random differential with decoder coverage, zero unexplained differences | met on 2026-10-15 | 20 480 blocks of 16 instructions (seeds 1-40, 512 blocks each), 0 differing words; every random-tested decoder entry executed at least 1 000 times; WFI, SVC, BKPT, UDF and undefined encodings are covered by directed cases only. After the inline fast path, 4 096 blocks again with 0 differences. No third reference was needed. Injected ADC bug detected (sanity check) |
| G0a.3 | ≥ 150 MIPS busy loop, ≥ 100 MIPS benchmark mix, Windows and Linux | Windows met; Linux to measure | 168-185 MIPS (HAL example busy loop), 119-141 MIPS (`tests/experiments/emu-benchmark`), idle machine |
| G0a.4 | Decoding never crashes the host | met on 2026-10-15 | `emu-unit`: all 65 536 16-bit patterns and 1 000 000 random 32-bit patterns decode to a valid operation of the right size; 200 random 1 KiB code images run 5 000 cycles each (faults, lockups, wild branches) without a host crash or hang |
| G0b.1 | Example firmware event sequences equal to Renode | met on 2026-10-15 | `emu-firmware` (`tests/unit/emu_firmware_compare.cpp`): blink, button_irq, uart_echo, lesson1_gpio, lesson2_button, hal_clock and freertos_tasks with scripted inputs give equal pin-transition and UART sequences; largest time difference 4-258 µs. Deviations, each with a third reference: Renode's SysTick period is RVR instead of RVR + 1 cycles (ADR 0083; ARMv7-M B3.3), so it drifts 1/(RVR + 1) (258 µs after 2 s of blink); the emulator's blink period is exactly 500.000 ms. The emulator's instruction-count timing is checked directly: 72 000 loop instructions take 1000.25 µs at 72 MHz including the GPIO calls |
| G0b.2 | Owner's CubeIDE project period within 0.1 % | **met on 2026-10-15** | Owner's STM32CubeMX/CubeIDE project with `HAL_Delay(200)` (72 MHz, HSE + PLL): PC13 toggles every 200.999958-201.000167 ms; HAL_Delay waits one extra tick, so 201 ms is the expected period on the chip; error below 0.0001 %. 122 MIPS, 1.70x real time |
| G0b.3 | FreeRTOS demo task-switch trace equal to Renode | met on 2026-10-15 | New example firmware `freertos_tasks` (FreeRTOS 10.3.1 from STM32CubeF1, three tasks, queue; every task switch writes the task tag to PB0-PB2): 63 pin transitions including every task switch and 54 UART bytes equal to Renode, within 124 µs, after correcting Renode's start-up deviation: when SysTick is enabled with CVR = 0, Renode counts the first period from 2^24 (2096.1 ms late at 8 MHz) instead of loading RVR on the next clock (ARMv7-M B3.3; FreeRTOS ticks after 1 ms on real chips) |
| G0b.4 | Fault and lockup diagnostics with PC and function | met on 2026-10-15 | `emu-unit`: a load from an unmapped address reports "HardFault: read from an address with no memory or peripheral (address 0x60000000) at PC 0x08000022 ... (in Reset_Handler)"; a fault inside the HardFault handler reports a lockup with "HardFault_Handler"; time keeps advancing, the host does not hang |
| G0b.5 | Reset kinds keep SRAM and set RCC_CSR flags | met on 2026-10-15 (IWDG later, SN-187) | `emu-unit`: NRST sets PINRSTF, SYSRESETREQ written to AIRCR sets SFTRSTF and PINRSTF, both keep SRAM and restart at Reset_Handler (PC13 low 500 ms after the reset); power-on clears SRAM and sets PORRSTF and PINRSTF; RMVF clears the flags |
| G0b.6 | ELF load under 50 ms; clean builds; differential test on both platforms | partly | Load and reset of blink.elf in 0.49 ms; clean MSVC and GCC builds |
| G0b.7 | Identical traces on repeated runs | met on 2026-10-15 (standalone) | `emu-unit`: hal_clock, lesson2_button and uart_echo with the same pin and UART inputs give byte-identical traces in two runs. The coupled-run version is a G1 criterion |

## Observations so far

- Blink example: PC13 periods of exactly 500.000 ms (SysTick period RVR + 1, as
  on the chip); Renode measures 125 ppm fast (ADR 0083).
- HAL example at 72 MHz: UART report SYSCLK 72 MHz, HCLK 72, PCLK1 36, PCLK2 72;
  a 72 000-instruction loop takes 1000.25 µs including the GPIO calls.
- Found by the comparison and fixed in SimNodus's own Renode GPIO model
  (`resources/renode/SimNodusGpio.cs`, shipped since 0.2 work): a BSRR write
  that resets and sets pins at once was applied as two steps, so the pins
  glitched low and high again at the same instant. On the chip the write is
  atomic; now the model applies it in one step.
- Renode deviation that affects the application today: FreeRTOS projects (and
  any firmware that enables SysTick with CVR = 0) get their first tick 2^24
  cycles late in Renode: 2.1 s at 8 MHz, 0.23 s at 72 MHz. Listed in
  MICROCONTROLLER.md; the own emulator does not have it.

## Interpretation and limitations

Preliminary. The timing model is one instruction per cycle, like Renode; the
cycle model comes later (plan N5).

## Decision and next step

Pending gates G0a and G0b (decision D0).
