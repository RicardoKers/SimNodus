# STM32F103 support matrix

Target direction: STM32F103C8/Blue Pill. E-02 validates the bounded offline firmware/digital-I/O profile; E-03 validates one restricted GPIO/RC/EXTI coupling path; E-04 validates a focused direct-voltage ADC path. None validates complete hardware or board fidelity.

| Feature | Initial need | Current evidence | Validation task |
|---|---|---|---|
| Cortex-M3, boot, memory | Required | Owned ELF booted twice in 64 KiB flash/20 KiB SRAM; startup data/BSS verified | SN-012 passed; broaden in SN-032 |
| GPIO output/input | Required | PA0 drove a real ngspice RC; threshold feedback reached PA1/EXTI and PA4 in E-03 | SN-014 passed replay and approximate sampled profile |
| GPIO mode/pulls/open-drain | Required subset | Mode bits/guards audited; pulls, analog isolation and open-drain electrical release not modeled | SN-013, SN-014 |
| EXTI/NVIC | Required for input lesson | E-03 handled distinct direct pulses down to 1 us in its profile; same-time edges collapsed; sampled feedback was late | Restricted E-03 profile; general causal feedback unsupported |
| SysTick/delay/clock setup | Required subset | Fixed 8 MHz SysTick generated 998-1000 us callback gaps; RCC only stores registers | SN-012 passed fixed profile; real clock tree SN-032 |
| Timers/PWM/alternate functions | Useful after GPIO | Models/routing visible; timing/fidelity untested | SN-012, SN-032 |
| ADC acquisition/conversion | Required for ADC lesson | Owned focused ADC1 subset passed direct integer-microvolt input, one 12-bit quantization, fixed timing/VREF, two-channel mapping, firmware ramp, and sample retention; electrical acquisition and complete modes unsupported | SN-015 passed focused E-04 profile; broaden in SN-032 |
| UART | Required for reports/lesson | Model declared; target firmware path untested | SN-012, SN-026 |
| SPI/I2C | Later coverage | Upstream declarations are not integration proof | SN-032 |
| DMA | Later coverage | Some upstream wiring exists; exact behavior untested | SN-032 |
| Flash programming/USB/CAN/RTC/watchdogs | Deferred | No SimNodus evidence | Future scoped tasks |
| GDB/CubeIDE | Required | GDB documented; coordinated circuit state untested | SN-016 |

Sources: the [pinned generic platform](https://github.com/renode/renode/blob/d66b0c2aa3d420408eccecfd1d3bab0fd702a6db/platforms/cpus/stm32f103.repl) remains unsuitable as a C8 declaration; [E-02](../experiments/E-02-results.md) uses a smaller offline subset and audits the matching pinned model sources. [E-04](../experiments/E-04-results.md) adds an owned experiment-only ADC instance after rejecting the incompatible generic ADC model. These subsets are intentionally incomplete and do not replace a product platform.

For each validated feature, add exact revision, owned test firmware, command, expected result, actual result, and limitations. Separate “model present,” “firmware runs,” and “electrical coupling validated.”
