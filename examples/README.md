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

MCU examples with firmware arrive in release 0.2 (lessons 1–3 of the
roadmap).
