# ADR 0088: Clock model for the STM32F103C8

Date: 2026-10-15. Status: accepted.

## Context

Renode 1.16.1 has no STM32F1 reset and clock control (RCC). The platform used
a plain register store at `0x40021000` and a fixed 8 MIPS CPU with an 8 MHz
SysTick (ADR 0083). The `SystemClock_Config()` that STM32CubeIDE generates
switches on HSE and the PLL and waits for their ready flags, then waits for
SWS to follow SW: with the store, the flags never came on, HAL timed out and
the firmware stopped in `Error_Handler()`. CubeIDE projects with the HAL are
the normal way students write firmware (MCU-01, MCU-08).

Two further gaps showed up with a real HAL build (STM32CubeF1 1.8.7): the F1
HAL switches HSION/PLLON/LSION through the Cortex-M3 bit-band alias
(`0x42000000`), which the platform did not map, and `HAL_RCC_ClockConfig()`
fails unless `FLASH_ACR` reads back the latency it wrote.

## Decision

- A C# peripheral `SimNodusSTM32F1RCC` (`resources/renode/SimNodusRcc.cs`,
  compiled by Renode at start-up like the GPIO model) replaces the store:
  ready flags follow the enable bits at once; SWS follows SW when the source is
  ready; SYSCLK and HCLK are computed from SW, PLLSRC, PLLXTPRE, PLLMUL and
  HPRE, with HSE = 8 MHz (the Blue Pill crystal, a platform parameter).
- **The CPU and SysTick follow HCLK.** The model sets the NVIC's SysTick
  frequency to HCLK and the CPU's `PerformanceInMips` to HCLK / 1 MHz (one
  instruction per cycle, at least 1 MIPS). Renode applies the new MIPS value
  when the CPU finishes its current time slice. Reset returns to 8 MHz.
- Renode's `BitBanding` maps the SRAM and peripheral bit-band aliases.
- `FLASH_ACR` and `PWR` (DBP for LSE/RTC set-up) are register stores.
- Example 12 (`hal_clock`) is built like a CubeIDE project from the
  STM32CubeF1 HAL (BSD-3-Clause) and CMSIS (Apache-2.0); the HAL is not
  vendored: `tools/build_firmware.py` takes it from the local STM32Cube
  repository, and the committed ELF carries the notices.

## Alternatives

- *Keep 8 MHz and tell students to skip `SystemClock_Config()`*: every
  CubeIDE project would need editing, and HAL timing would be wrong after
  copying code to a real board.
- *Report ready flags but keep 8 MIPS*: `HAL_Delay()` would still be right
  (SysTick follows HCLK), but busy loops and instruction-timed code would run
  nine times slower than on the board. Following HCLK is closer to the chip;
  the cost is simulation speed.
- *Model flash wait states and branch penalties (cycles per instruction)*:
  Renode's translation CPU has no cycle model; not available.

## Consequences

- Measured: the HAL example prints SYSCLK 72 MHz, HCLK 72 MHz, PCLK1 36 MHz,
  PCLK2 72 MHz; PC13 toggles every 500.0 ms of HAL ticks; 72000 instructions
  take exactly 1000 µs. The blink firmware's SysTick runs nine times faster
  after a register-level switch to PLL x9 (55.55 ms per toggle) and returns
  to 500 ms after a reset.
- Firmware at 72 MHz costs nine times more emulation per simulated second.
  Example 12 (busy-waiting in its main loop) runs at about 0.85x real time on
  the owner's laptop; standalone Renode reaches 1.2x. The status bar shows the
  speed. Examples at 8 MHz are unchanged.
- Peripheral clock gating and start-up times are not modeled; peripherals
  work without their clock enable (documented in MICROCONTROLLER.md).
- ADR 0083's SysTick deviation (RVR instead of RVR + 1 cycles) remains,
  smaller at 72 MHz (14 ppm).

## Revisit criteria

Firmware that depends on clock gating or start-up times; timers and PWM
(SN-141) and the ADC (SN-140), which need the APB prescalers; classroom
complaints about speed with 72 MHz firmware (a larger coupling quantum
or a WFI-based idle in examples would help).
