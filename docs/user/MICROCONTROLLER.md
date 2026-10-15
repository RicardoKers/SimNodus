# Microcontroller: what is simulated

SimNodus runs the firmware of the **Blue Pill (STM32F103C8)** in Renode 1.16.1
next to the circuit. This page lists what the simulated chip supports and
where it differs from a real board (MCU-09). Release 0.2.

## Supported

| Part | Status | Notes |
|---|---|---|
| Cortex-M3 core, 64 KiB flash, 20 KiB SRAM | yes | ELF files built for the STM32F103C8 (STM32CubeIDE, arm-none-eabi-gcc) |
| Clock | fixed 8 MHz | The reset clock (HSI 8 MHz) at 8 MIPS. RCC registers only store what is written: the HSE and PLL **ready flags never come on**, so firmware that waits for them — the `SystemClock_Config()` that STM32CubeIDE generates with HAL — waits forever. Keep the default clock (HSI 8 MHz) for now; a clock model is planned |
| SysTick | yes | About 125 ppm fast (counts RVR instead of RVR + 1 cycles) |
| GPIO ports A, B, C | yes | Modes from CRL/CRH, ODR/BSRR/BRR/IDR; push-pull, open-drain, inputs with pull-up/pull-down |
| GPIO outputs into the circuit | yes | 3.3 V or 0 V behind 40 Ω; open drain only pulls low |
| GPIO inputs from the circuit | yes | Schmitt trigger: 1 above 1.83 V, 0 below 1.16 V; floating inputs read 0 |
| EXTI 0–15 and NVIC | yes | Lines reach IRQ 6–10, 23 (5–9) and 40 (10–15) as on the chip |
| USART1–3 | yes | Text in the Signal Analyzer's UART tab |
| On-board LED (PC13, lit when low) and RESET button | yes | Clickable while running |

## Not yet simulated

| Part | Planned |
|---|---|
| ADC (analog inputs) | Release 0.3 |
| Timers / PWM outputs | Release 0.3, after an audit of the Renode model |
| SPI, I2C, CAN, USB, DMA, RTC, watchdogs | Not planned yet; the firmware's register accesses have no effect |
| Debugging with STM32CubeIDE / GDB | Release 0.4 |
| Transient mode with a microcontroller | Not yet: use interactive mode |

## Differences from the real board

- **Timing between firmware and circuit.** The firmware runs at most two
  *microcontroller quanta* (Simulate > Simulation Settings, default 1 ms)
  ahead of the circuit. Output changes reach the circuit at their exact
  firmware time (rounded to Renode's 100 µs time step). Circuit changes
  reach the firmware up to two quanta late, in order and with their
  spacing. The status bar tooltip states these approximations.
- **AFIO.** EXTI line *n* follows pin *n* of every port; the port selection
  in `AFIO_EXTICR` is not simulated. Use one pin number per EXTI line.
- **UART.** Bytes move instantly: the baud rate is not simulated and the TX
  pin does not carry the serial waveform.
- **Pin speed and drive.** Edge speed (MODE 10/2/50 MHz) and current limits
  are not simulated; an LED without a resistor draws an unrealistic current.
- **Power.** The board is fed by its 3.3 V regulator (an ideal 3.3 V source
  behind 0.1 Ω); 5 V and VBAT pins are not connected inside.

## Building firmware

Any STM32F103C8 project works if it produces an ELF. The examples in
`examples/firmware/` use no libraries and build with
`python tools/build_firmware.py` (finds the compiler of an STM32CubeIDE
installation or `arm-none-eabi-gcc` on PATH). Projects store the ELF path
relative to the project file; **Reload** in Properties picks up a rebuilt ELF.

Technical background: ADR 0082–0086 in `docs/decisions/`.
