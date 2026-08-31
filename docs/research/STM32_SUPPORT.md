# STM32F103 support matrix

Target direction: STM32F103C8/Blue Pill. **No feature has been validated in SimNodus.** The exact part, board wiring, memory map, clocks, and platform revision must be fixed in E-02.

| Feature | Initial need | Current evidence | Validation task |
|---|---|---|---|
| Cortex-M3, boot, memory | Required | Upstream platform exists; exact C8 mapping unverified | SN-012 |
| GPIO output/input | Required | Upstream GPIO model declared; electrical mode coverage unverified | SN-012, SN-014 |
| GPIO mode/pulls/open-drain | Required subset | Callback-level logic is insufficient evidence | SN-012, SN-013 |
| EXTI/NVIC | Required for input lesson | Generic platform wiring visible; behavior untested | SN-014 |
| SysTick/delay/clock setup | Required subset | Firmware startup and clock assumptions untested | SN-012 |
| Timers/PWM/alternate functions | Useful after GPIO | Models/routing visible; timing/fidelity untested | SN-012, SN-032 |
| ADC acquisition/conversion | Required for ADC lesson | No ADC instance visible in inspected generic file; exact setup pending | SN-015 |
| UART | Required for reports/lesson | Model declared; target firmware path untested | SN-012, SN-026 |
| SPI/I2C | Later coverage | Upstream declarations are not integration proof | SN-032 |
| DMA | Later coverage | Some upstream wiring exists; exact behavior untested | SN-032 |
| Flash programming/USB/CAN/RTC/watchdogs | Deferred | No SimNodus evidence | Future scoped tasks |
| GDB/CubeIDE | Required | GDB documented; coordinated circuit state untested | SN-016 |

Source: [pinned Renode 1.16.1 platform](https://github.com/renode/renode/blob/d66b0c2aa3d420408eccecfd1d3bab0fd702a6db/platforms/cpus/stm32f103.repl), inspected and hash-checked in SN-010 on 2026-08-31. It is a generic file, not a complete declaration of the selected board or all supported behaviors. It has oversized flash/SRAM maps, no ADC instance, an external SVD download in initialization, and a fixed RCC tag. E-02 must establish an offline C8 profile before interpreting firmware behavior as target support. No platform was loaded in SN-010.

For each validated feature, add exact revision, owned test firmware, command, expected result, actual result, and limitations. Separate “model present,” “firmware runs,” and “electrical coupling validated.”
