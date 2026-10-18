# Example projects

Analog lesson circuits for release 0.1, opened from File > Examples as
untitled copies. Each contains probes for the Signal Analyzer. A unit test
([examples_tests](../tests/unit/examples_tests.cpp)) checks that every example
opens and passes the pre-run checks without errors or warnings.

| File | Circuit | What to try |
|---|---|---|
| `01_LED_and_push_button.snx` | 5 V, push button, 330 Ω, red LED | Hold the button: the LED lights (about 9 mA) |
| `02_RC_filter_with_pulse.snx` | 2 Hz pulse into 10 kΩ / 10 µF | Watch charge and discharge (τ = 0.1 s) in the analyzer |
| `03_Potentiometer_and_meters.snx` | 9 V battery, ammeter, 10 kΩ potentiometer, voltmeter | Turn the wiper; the voltmeter follows, the current stays 0.9 mA |
| `04_Transistor_switch.snx` | Switch, 10 kΩ base resistor, NPN, green LED | Toggle the switch; base voltage and LED current on the scope |
| `05_RC_charging_transient.snx` | 5 V step into 1 kΩ / 10 µF, transient 50 ms | F5: the complete curve opens in the analyzer; use the cursors at τ = 10 ms |
| `06_Half_wave_rectifier.snx` | 5 V 50 Hz sine, diode, 1 kΩ load, 47 µF | Ripple on the scope (10 ms/div); delete C1 and compare |
| `08_Blue_Pill_button_interrupt.snx` | Blue Pill running `firmware/button_irq/button_irq.elf`, push button PA1–G, 330 Ω and green LED on PA0 | Each press toggles the LED from the EXTI1 interrupt (about 1 ms response); PC13 lights while held |
| `09_Blue_Pill_serial_terminal.snx` | Blue Pill running `firmware/uart_echo/uart_echo.elf` (USART1, 115200 baud on a real board), LED on PA0 | Analyzer > UART: "tick N" every second; send `1` / `0` to switch the LED |
| `10_Lesson1_GPIO_output.snx` | **Lesson 1** ([notes](lessons/lesson1_gpio_output.md)): LED on PA0, 1 kHz square wave on PA1 into a 10 kΩ / 100 nF RC filter | V(PA1), V(RC) ramps of about 0.8 V peak-to-peak, LED current 3.8 mA |
| `11_Lesson2_button_and_EXTI.snx` | **Lesson 2** ([notes](lessons/lesson2_button_exti.md)): bouncing push button with 10 kΩ pull-up on PA1, switchable 1 µF RC filter, LED on PA0, EXTI1 counter on USART1 | Without the filter one press and release count 9 falling edges (5 on press, 4 on release); with it exactly one |
| `16_Lesson3_ADC_and_UART.snx` | **Lesson 3** ([notes](lessons/lesson3_adc_uart.md)): potentiometer on PA2 (ADC1 IN2), PWM 1 kHz on PA0 into a 10 kΩ / 1 µF RC filter read back on PA1 (IN1); single conversions with EOC polling; report on USART1 | "POT 2048 = 1650 mV  duty 50.0 %  RC 1621 mV" in the UART tab; turn the potentiometer and watch V(RC) settle |
| `12_Blue_Pill_HAL_72MHz.snx` | Blue Pill running `firmware/hal_clock/hal_clock.elf`, an STM32CubeIDE-style HAL project at 72 MHz (HSE + PLL x9); 330 Ω and yellow LED on PA1 | Analyzer > UART shows SYSCLK 72000000 Hz; PC13 blinks every 500 ms; PA1 pulses for 1 ms (72000 instructions at 72 MHz) every 500 ms |
| `13_Blue_Pill_timer_PWM.snx` | Blue Pill running `firmware/timer_pwm/timer_pwm.elf` (registers, no HAL): TIM2 PWM 1 kHz / 25 % on PA0 into 10 kΩ / 1 µF; PA0 wired back to PB6, where TIM4 captures the period; TIM3 interrupt toggles PC13 every 10 ms; TIM1 PWM with complementary output on PA8/PB13 | V(RC) settles at 0.825 V (a quarter of 3.3 V) with a small ripple; the green LED on PB12 lights when TIM4 measures 0.9–1.1 ms; probe PA8 and PB13 |
| `14_Blue_Pill_ADC_to_PWM.snx` | Blue Pill running `firmware/adc_pwm/adc_pwm.elf` (registers, no HAL): 10 kΩ potentiometer between 3.3 V and ground, wiper on PA1 (ADC1 IN1, continuous conversions with interrupt); the code sets the duty of a 1 kHz PWM on PA0 into an LED and a 10 kΩ / 1 µF filter; USART1 reports the code | Turn the potentiometer: the LED dims and brightens, V(RC) follows (half of 3.3 V at the middle), Analyzer > UART shows "ADC 2048 = 1650 mV"; PC13 lights above the middle |
| `15_Blue_Pill_HAL_ADC_DMA.snx` | The circuit of example 14 with `firmware/hal_adc_dma/hal_adc_dma.elf`, an STM32CubeIDE-style HAL project at 72 MHz: ADC1 scans PA1 and VREFINT continuously into memory by circular DMA (`HAL_ADC_Start_DMA`), TIM2 PWM (`HAL_TIM_PWM_Start`), reports with `HAL_UART_Transmit_DMA` | Analyzer > UART: "PA1 2048 VREFINT 1489 groups ..." every 500 ms (23 800 scans per second); turn the potentiometer: the LED and V(RC) follow within 0.1 s |
| `17_Logic_gates_half_adder.snx` | Half adder: switches A and B with 10 kΩ pull-downs into an XOR gate (sum) and an AND gate (carry), 5 V gates, red LED on SUM and green LED on CARRY | Toggle the switches: one closed lights SUM, both light CARRY only; V(A), V(B), V(SUM), V(CARRY) on the scope or the Logic tab |
| `18_Bus_with_splitters.snx` | Four SPDT switches put VCC or ground on D0..D3 through a splitter; bus D carries them to a second splitter that drives four LEDs through 330 Ω | Click the switches: each LED follows its bit; the probes show V(D0)..V(D3) on the scope or the Logic tab |
| `07_Blue_Pill_blink.snx` | Blue Pill running `firmware/blink/blink.elf`, 330 Ω and red LED on PA0 | PC13 LED blinks every 500 ms, PA0..PA3 count, the LED on PA0 blinks at 2 Hz (about 3.8 mA); click RESET |

`firmware/freertos_tasks` (FreeRTOS 10.3.1 under the MIT license, from the
STM32CubeF1 package; three tasks, a queue, task switches on PB0-PB2) is test
firmware for the emulator experiment E-07; an example project follows.

Firmware sources are in `firmware/<name>/` and link with
`firmware/stm32f103c8.ld` without libraries, except `hal_clock` and
`hal_adc_dma`, which are built like STM32CubeIDE projects from the STM32CubeF1 HAL (`cube.json`
names the files; `firmware/stm32f103c8_cube.ld`; notices in the package's
THIRD-PARTY-NOTICES.txt); the ELF files are committed so
that the examples run without a compiler. `python tools/build_firmware.py`
rebuilds them (arm-none-eabi-gcc on PATH, or the GNU Tools for STM32 of an
STM32CubeIDE installation) and records compiler and hashes in
`firmware/firmware.json`.
