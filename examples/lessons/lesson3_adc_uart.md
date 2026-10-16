# Lesson 3 — ADC and UART, PWM into an RC filter

Project: `examples/16_Lesson3_ADC_and_UART.snx` · Firmware:
`examples/firmware/lesson3_adc/lesson3_adc.c` (ELF next to it).

## Goals

- Measure a voltage with the 12-bit ADC: reference, resolution (LSB),
  channel selection, sample time, single conversion with EOC polling.
- Convert the code to millivolts and report it over UART.
- Generate a PWM with a timer and see that an RC low-pass filter turns its
  duty cycle into a DC voltage, with a ripple.

## Circuit

- RV1, 10 kΩ potentiometer, between 3.3 V and ground; its wiper (node POT)
  goes to PA2 = ADC1 channel IN2.
- PA0 = TIM2 channel 1: PWM 1 kHz → R1 10 kΩ → node RC → C1 1 µF → ground
  (τ = 10 ms, ten times the PWM period).
- Node RC also goes to PA1 = ADC1 channel IN1, so the firmware measures the
  filtered PWM.

## Firmware

The internal 8 MHz clock runs the CPU; the ADC clock is 8 MHz / 2 = 4 MHz.
After power-on (`ADON`) the ADC is calibrated (`CAL`). Each measurement writes
the channel number to `ADC1_SQR3`, starts the conversion with `SWSTART`, waits
for `EOC` and reads `ADC1_DR`. The sample time is the longest, 239.5 cycles;
with the 12.5 conversion cycles a measurement takes 63 µs.

Every 200 ms the firmware converts IN2 (potentiometer) and IN1 (RC), sets
the PWM duty to code / 4096, and prints one line on USART1:

    POT 2048 = 1650 mV  duty 50.0 %  RC 1621 mV

Millivolts are `code × 3300 / 4096`, rounded: one LSB is 3.3 V / 4096 =
0.806 mV.

## Steps

1. Run the project (F5) and open the Signal Analyzer: the **UART** tab, and
   the Scope tab with V(POT), V(PWM) and V(RC) at 1 ms/div.
2. Read the first lines with the potentiometer in the middle (50 %).
3. Turn the potentiometer (drag it, or use the mouse wheel over it) to about
   25 % and to 90 %.
   Watch the code, the duty and V(RC) settle.
4. On the scope, look at V(RC): a small sawtooth around its average. Zoom
   into V(PWM).
5. Turn on **Simulate > Simulation Settings > Exact microcontroller edges**,
   run again and compare V(PWM) and V(RC) on the scope.
6. Change C1 to 100 nF and run again.

## Expected observations

- At 50 % the wiper is at 1.65 V and the code is 2048 (1650 mV); at 25 %,
  1024 (825 mV). The code changes by one for every 0.806 mV.
- The duty follows the potentiometer within 200 ms; V(RC) reaches its new
  average after about 5τ = 50 ms.
- V(RC) averages duty × 3.3 V but ripples by about ±40 mV at 50 %. The
  firmware measures RC every 200 ms, a whole number of PWM periods, so it
  always catches the same point of the ripple: the reported RC value is
  about 30 mV below duty × 3.3 V, the same in every line.
- Without exact edges each PWM edge is spread over one 0.1 ms time step on
  the scope; with them the square wave is square and the run is slower.
- With C1 = 100 nF (τ = 1 ms) the ripple is ten times larger (about
  ±0.4 V at 50 %) and the RC value jumps around depending on the phase.

## Questions

1. What is the smallest voltage step this ADC can see? How many bits would
   you need for 0.1 mV?
2. Why does the RC value stay a little below duty × 3.3 V? How could the
   firmware measure the true average (hint: several samples spread over one
   PWM period, or a larger capacitor)?
3. Why must τ be much larger than the PWM period? What does a larger τ cost?
4. The sample time is 239.5 cycles. When does a short sample time give a
   wrong reading? (Think of the resistance of the source: here the
   potentiometer gives up to 2.5 kΩ.)

## Limits of the simulation

The ADC reads the voltage the circuit had 0.1–0.2 ms before the sample
instant (ADR 0092), which does not matter for these slow signals. The
ADC input is ideal: it does not load the circuit and has no noise; a real
STM32 reads a few LSB of noise and needs a source impedance below about
50 kΩ at the longest sample time. VDDA is exactly 3.3 V.
