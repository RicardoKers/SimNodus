# STM32F407 test firmware (experiment E-12)

Firmware of the STM32F407VET6 track
([plan](../../../docs/experiments/E-12-plan.md),
[results](../../../docs/experiments/E-12-results.md)): each folder
`f4_<name>` has one C file and the ELF it was built to. They use no library
and no HAL (except `f4_hal_buses`, below): `f4.h` here has the register addresses, the vector table and the
start-up code; `f4_peripherals.h` adds I2C1 as a master, lines of text sent
on USART1 through DMA2 and the start of the RTC. `tools/build_f4_firmware.py` builds them with the
arm-none-eabi-gcc of STM32CubeIDE for the Cortex-M4 with its FPU
(`f4_firmware.json` records the compiler, the flags and each ELF's SHA-256).

| Firmware | Clock | What it does |
|---|---|---|
| `f4_blink` | HSI 16 MHz | PC13 toggles every 500 ms, PA0..PA3 count every 250 ms, PB0 toggles after a busy loop of 1 000 000 turns |
| `f4_clock` | 25 MHz crystal, PLL at 168 MHz | prints `SYSCLK 168000000 HCLK 168000000 PCLK1 42000000 PCLK2 84000000`, then `tick <n>` every second, on USART1 (PA9) at 115200 baud; PC13 toggles every 500 ms |
| `f4_float` | HSI 16 MHz | floating point and DSP instructions with an interrupt that uses the FPU; prints five checksum lines and `done` on USART1 (PA9) at 115200 baud |
| `f4_button` | HSI 16 MHz | a push button from PA0 to ground: PC13 is high while it is pressed, PB0..PB3 count the presses |
| `f4_analog` | HSI 16 MHz | TIM2 starts ADC1 on PA1 every millisecond; the code sets the duty cycle of TIM1's 20 kHz PWM on PA8 and its complement on PB13 (0.5 us of dead time) and DAC channel 1 on PA4; DAC channel 2 draws a triangle on PA5 (TIM6, 100 us a step); prints `adc <code>` every 100 ms on USART1 (PA9) at 115200 baud |
| `f4_buses` | HSI 16 MHz | SPI1 (PA5, PA7; PA4 the latch) counts on a 74HC595 every 250 ms; SPI2 (PB13, PB15, 16-bit frames; PB12 LOAD) draws a smiley on a MAX7219; I2C1 (PB6, PB7) writes `SimNodus` into a 24C02 at 0x50 and reads it back, then reads an LM75 at 0x48 every second; prints `f4_buses`, `eeprom SimNodus`, `lm75 23.5` on USART1 through DMA2 |
| `f4_hal_buses` | HSI 16 MHz | `f4_buses` written as an STM32CubeIDE project with the STM32CubeF4 HAL (`main.c`, `stm32f4xx_hal_conf.h`, `cube.json`: the package's files it is built with, newlib-nano, `stm32f407ve_cube.ld`); USART1 through `HAL_UART_Transmit_DMA`; prints `f4_hal_buses`, then the same lines. It runs on example 69's circuit with U1's firmware changed |
| `f4_rtc` | HSI 16 MHz; RTC on the LSE (32.768 kHz on PC14, PC15) or the LSI | sets the calendar to 2026-11-12 12:00:00 on its first start, then prints the time every second (wakeup timer) and `alarm` at second 05; backup register 1 counts the starts; the independent watchdog (2 s) resets the chip while PA0 is high; PC13 toggles every second |
| `f4_station` | 25 MHz crystal, PLL at 168 MHz; RTC as `f4_rtc` | sleeps until the RTC's second, reads an LM75 on I2C1, sets TIM3's 1 kHz PWM on PA6 (10 counts of 1000 per degree) and the DAC on PA4 (40 codes per degree), prints `12:00:01 2026-11-12 lm75 23.5 pwm 235 dac 940` through DMA2 |
| `f4_timing` | HSI 16 MHz, then the 25 MHz crystal and the PLL at 168 MHz | counts the core cycles of 27 loops (integer, loads and stores, divides, DSP, FPU, a 2 KiB loop, C code, interrupts) with DWT CYCCNT at 16 MHz and at 168 MHz with 5 wait states in three ART accelerator setups; prints them every 3 s |

## In the simulator

Examples 67 (`f4_clock`), 68 (`f4_analog`), 69 (`f4_buses`), 70 (`f4_rtc`)
and 71 (`f4_station`) run them on the diagram;
the command line runs them without a circuit:

```
simnodus-emu --stm32f407 examples/firmware/f4_float/f4_float.elf --ms 300 --quiet
simnodus-emu --stm32f407 examples/firmware/f4_clock/f4_clock.elf --crystal 25000000 --ms 2100 --quiet
simnodus-emu --stm32f407 examples/firmware/f4_blink/f4_blink.elf --ms 1100
simnodus-emu --stm32f407 examples/firmware/f4_button/f4_button.elf --input PA0=0@100 --input PA0=1@200
simnodus-emu --stm32f407 examples/firmware/f4_timing/f4_timing.elf --crystal 25000000 --ms 400 --quiet
```

## On a real chip (the comparison of gate G12b)

Write an ELF with STM32CubeProgrammer (or `Run > Debug` in STM32CubeIDE
with the ELF as the application) through an ST-LINK on SWD. Connect a
USB-serial adapter's RX to **PA9** and its ground to the board's; open a
terminal at 115200 baud, 8N1. Press reset to see the output from the start.

What the simulator prints, to compare:

- `f4_float` (this is the comparison that matters most: the same numbers
  must come out of the chip):

  ```
  A 1A3EE223
  B 183ECD01
  C 596EC23A
  I 46B57D98
  x 40182DAE
  done
  ```

- `f4_clock` with the 25 MHz crystal: the `SYSCLK` line above and one
  `tick` a second. On a board with another crystal the text is the same
  but the baud rate is not 115200 (the firmware assumes 25 MHz): the
  terminal shows garbage, and so does the simulator's pin.
- `f4_blink`: with a logic analyzer, PC13 at 1 Hz and PA0 at 2 Hz; the
  period of PB0 is the busy loop's cycle count (the simulator gives
  501.25 ms a half period, 8.02 cycles a turn; the real number is what the
  timing model of SN-296 will be calibrated with).
- `f4_button`: a wire from PA0 to ground as the button.
- `f4_timing` (the calibration of the timing model, G12b.9): the 25 MHz
  crystal. It prints four blocks of `name cycles` lines, `f4_timing v1` to
  `end`, again every 3 s; copy one whole report from the terminal. The
  simulator's first block (16 MHz, no wait state) has the Cortex-M4's
  manual counts, for example `alu 11002`, `vadd 11003`, `vdiv 115003`; its
  168 MHz blocks are still a guess (no wait state with the accelerator on,
  all five with it off) that the board's numbers will replace.
- `f4_analog`: a potentiometer between 3.3 V and ground with its wiper on
  PA1. At 1.00 V the simulator prints `adc 1241`; with the logic analyzer
  on PA8 and PB13 each edge comes 0.5 us after the other's, and PA8 is high
  for (code × 800 / 4096 − 8) / 16 us of every 50 us. A voltmeter on PA4
  reads PA1's voltage (within the DAC's buffer range, 0.2 V to 3.1 V); PA5
  is a triangle of about 4.9 Hz between 1.24 V and 2.06 V.
- `f4_buses`: a 24C02 (or any 24Cxx) at 0x50 and an LM75 at 0x48 on PB6 and
  PB7 with 4.7 kΩ pull-ups; a 74HC595 on PA4, PA5, PA7 and a MAX7219 on
  PB12, PB13, PB15 as in example 69. The lines `eeprom SimNodus` and
  `lm75 <temperature>`; with the logic analyzer, SPI1's clock at 1 MHz and
  SPI2's at 2 MHz. The SPI pins are busy only in short bursts (about 10 us
  every 250 ms, plus the MAX7219's setup every second): on a scope use
  normal or single trigger on the rising edge of PA5 or PB13 at 2-5 us per
  division; in auto mode the line looks idle. A MAX7219 or a 74HC595 on
  5 V needs 3.5 V (MAX7219) or 3.15 V (74HC at 4.5 V) as a high input, at
  or above the STM32's 3.3 V: power the 74HC595 from 3.3 V (or use a
  74HCT595) and, if the matrix stays dark, check the MAX7219's input levels.
  A 24C08 answers at 0x50 to 0x53 (A2 to ground, WP to ground for writes);
  0x50 is its first block, so it works as the 24C02 here.
- `f4_rtc`: with the board's 32.768 kHz crystal the first line after
  `start 1` comes one second after the calendar is set; the seconds of the
  board and of the simulator must not drift apart by more than the
  crystal's tolerance (20 ppm: 0.07 s an hour). Without the crystal the
  firmware says `(LSI)`, whose frequency varies from chip to chip (17 to
  47 kHz in DS8626): a measured drift is the chip's, not the simulator's.
  A wire from 3.3 V to PA0 for 2 s: `reset by the watchdog` and `start 2`.
- `f4_station` (gate G12c.7): the 25 MHz crystal, an LM75 on PB6 and PB7.
  The serial lines must match the simulator's byte for byte (with the
  board's temperature); PA6's duty cycle within one count of 1000; PA4
  within 1 % of 3.3 V of 3.3 × code / 4095.

The simulator's numbers are not confirmed by a chip yet: a difference is a
finding to report, not a mistake of the bench.
