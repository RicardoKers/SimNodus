# SimNodus quick start

Five minutes from installation to a running circuit. Everything else is in
the [user guide](USER_GUIDE.md) (Help > User Guide), including its
[known limitations](USER_GUIDE.md#known-limitations).

## 1. Install

Run `SimNodus-<version>-windows-x64-setup.exe` (for you alone without
administrator rights, or for all users), or unzip the portable package and
run `simnodus.exe`. Both work offline.

## 2. Open an example

File > Examples > **01 LED and push button**, then press **F5**. Hold the
mouse button on the push button: the LED lights. Shift+F5 stops.

## 3. Draw your own circuit

An LED on a 5 V source:

1. In **Components** (left), search *DC voltage source* and click on the
   schematic to place it; Esc ends placing. Do the same with a **resistor**
   (key R), an **LED** and a **ground** (key G).
2. Select the resistor and type `330` in **Properties** > Resistance, then
   Enter. Values accept `4k7`, `100n`, `2.2u`, `1meg`.
3. Press **W** and click from pin to pin: source + to the resistor, the
   resistor to the LED's anode, the LED's cathode and source − to ground.
   A red circle marks a pin that is still unconnected.
4. Press **F5**. The LED glows; select it to see its current in Properties,
   or hover a wire to see its voltage.

## 4. Look at the signals

Press **P** and click a wire: a voltage probe. Window > Signal Analyzer
(Ctrl+Shift+A) shows it on the scope; try a pulse generator instead of the
DC source (example 02).

## 5. Run firmware

File > Examples > **07 Blue Pill blink**, then F5: the board's PC13 LED and
the LED on PA0 blink. In your own circuit, place **Blue Pill (STM32F103C8)**,
ground a G pin and choose your `.elf` with **Browse...** in Properties. See
the [microcontroller page](MICROCONTROLLER.md).

## Keys to remember

| Key | Action |
|---|---|
| F1 | All shortcuts |
| F5 / F6 / Shift+F5 | Run / pause / stop |
| W, N, P, T | Wire, net label, probe, text note |
| R, C, L, D, G | Resistor, capacitor, inductor, diode, ground |
| V, I, Shift+V, Shift+I | Voltage source, current source, voltmeter, ammeter |
| Ctrl+R, Ctrl+E | Rotate, mirror |
| F | Fit the circuit in the window |
