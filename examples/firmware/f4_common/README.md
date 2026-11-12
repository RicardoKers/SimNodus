# STM32F407 test firmware (experiment E-12)

Firmware of the STM32F407VET6 track
([plan](../../../docs/experiments/E-12-plan.md),
[results](../../../docs/experiments/E-12-results.md)): each folder
`f4_<name>` has one C file and the ELF it was built to. They use no library
and no HAL: `f4.h` here has the register addresses, the vector table and the
start-up code. `tools/build_f4_firmware.py` builds them with the
arm-none-eabi-gcc of STM32CubeIDE for the Cortex-M4 with its FPU
(`f4_firmware.json` records the compiler, the flags and each ELF's SHA-256).

| Firmware | Clock | What it does |
|---|---|---|
| `f4_blink` | HSI 16 MHz | PC13 toggles every 500 ms, PA0..PA3 count every 250 ms, PB0 toggles after a busy loop of 1 000 000 turns |
| `f4_clock` | 25 MHz crystal, PLL at 168 MHz | prints `SYSCLK 168000000 HCLK 168000000 PCLK1 42000000 PCLK2 84000000`, then `tick <n>` every second, on USART1 (PA9) at 115200 baud; PC13 toggles every 500 ms |
| `f4_float` | HSI 16 MHz | floating point and DSP instructions with an interrupt that uses the FPU; prints five checksum lines and `done` on USART1 (PA9) at 115200 baud |
| `f4_button` | HSI 16 MHz | a push button from PA0 to ground: PC13 is high while it is pressed, PB0..PB3 count the presses |

## In the simulator

Until the chip is a part on the diagram (SN-295), the command line runs
them:

```
simnodus-emu --stm32f407 examples/firmware/f4_float/f4_float.elf --ms 300 --quiet
simnodus-emu --stm32f407 examples/firmware/f4_clock/f4_clock.elf --crystal 25000000 --ms 2100 --quiet
simnodus-emu --stm32f407 examples/firmware/f4_blink/f4_blink.elf --ms 1100
simnodus-emu --stm32f407 examples/firmware/f4_button/f4_button.elf --input PA0=0@100 --input PA0=1@200
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

The simulator's numbers are not confirmed by a chip yet: a difference is a
finding to report, not a mistake of the bench.
