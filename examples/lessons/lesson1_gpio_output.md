# Lesson 1 — GPIO output

Project: `examples/10_Lesson1_GPIO_output.snx` · Firmware:
`examples/firmware/lesson1_gpio/lesson1_gpio.c` (ELF next to it).

## Goals

- Configure STM32F103 pins as push-pull outputs by writing `GPIOx_CRL/CRH`.
- Drive an LED through a series resistor and size the resistor.
- See what a digital output looks like to an analog circuit: an RC low-pass
  filter turns a square wave into exponential ramps.

## Circuit

- PA0 → R1 330 Ω → red LED → ground.
- PA1 → R2 10 kΩ → node RC → C1 100 nF → ground (τ = R·C ≈ 1 ms).
- The board's G pin is tied to ground; the board feeds itself from its
  3.3 V regulator.

## Firmware

SysTick interrupts every 0.5 ms (8 MHz / 4000). Each interrupt toggles PA1
(1 kHz square wave); every 500 ms it switches the LED on PA0 and mirrors it
on the on-board LED PC13 (lit when PC13 is low). No library is used: the
registers are written directly, as in the reference manual (RM0008).

## Steps

1. Open the project (File > Examples) and press **F5**. The board starts in a
   few seconds; the LEDs blink at 1 Hz.
2. Open the **Signal Analyzer** (Ctrl+Shift+A), set 1 ms/div. Channels:
   V(PA1), V(RC), I(LED1.A).
3. Switch to 200 ms/div and watch the LED current.
4. Open the **Logic** tab: PA0, PA1 and PC13 appear as lanes.
5. Change C1 to 1 µF and R2 to 1 kΩ (stop, edit, run again) and compare.

## Expected observations

- V(PA1): 0 V / 3.3 V square wave, 1 kHz, duty 50 %.
- V(RC): ramps between about 1.25 V and 2.05 V around 1.65 V. Theory for a
  square wave of amplitude V into an RC with τ, half period T/2:
  peak-to-peak = V·tanh(T/4τ) = 3.3·tanh(0.25) ≈ 0.80 V. The simulation
  gives 0.79–0.80 V.
- LED current about (3.3 V − 1.9 V)/(330 Ω + 40 Ω) ≈ 3.8 mA while lit.
- With τ = 10 ms the ripple shrinks to about 0.08 V: the filter averages the
  square wave to 1.65 V (this is how PWM becomes an analog voltage).

## Questions

1. Which resistor gives 10 mA in the red LED? Check it in the simulation.
2. Why does V(RC) settle around 1.65 V?
3. What changes on the real board if the GPIO speed bits (MODE) are 10 MHz
   instead of 2 MHz? (Hint: edge speed; the simulation does not model it.)

## Limits of the simulation

Output edges fall on Renode's 100 µs time quantum; the pin is modeled as
3.3 V / 0 V behind 40 Ω (ADR 0084). Renode's SysTick is 125 ppm fast.
