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
| `07_Blue_Pill_blink.snx` | Blue Pill running `firmware/blink/blink.elf`, 330 Ω and red LED on PA0 | PC13 LED blinks every 500 ms, PA0..PA3 count, the LED on PA0 blinks at 2 Hz (about 3.8 mA); click RESET |

Firmware sources are in `firmware/<name>/` and link with
`firmware/stm32f103c8.ld` without libraries; the ELF files are committed so
that the examples run without a compiler. `python tools/build_firmware.py`
rebuilds them (arm-none-eabi-gcc on PATH, or the GNU Tools for STM32 of an
STM32CubeIDE installation) and records compiler and hashes in
`firmware/firmware.json`. Lesson examples 1–3 follow in release 0.2.
