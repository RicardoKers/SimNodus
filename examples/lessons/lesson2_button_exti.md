# Lesson 2 — Button input, bounce and EXTI

Project: `examples/11_Lesson2_button_and_EXTI.snx` · Firmware:
`examples/firmware/lesson2_button/lesson2_button.c` (ELF next to it).

## Goals

- Read a push button with a pull-up resistor; understand active-low inputs.
- Use an external interrupt (EXTI1) on the falling edge.
- See contact bounce on the scope and in the firmware's count, and remove it
  with an RC filter.

## Circuit

- 3.3 V → R2 10 kΩ pull-up → node BTN → PA1 (floating input in the firmware:
  the external resistor decides the level).
- SW1: push button from BTN to ground, **Contact bounce = yes** (its contacts
  bounce for about 3 ms after each press and release).
- SW2 + C1 1 µF: when SW2 is closed, C1 sits across the button — an RC filter
  with τ = 10 kΩ · 1 µF = 10 ms.
- PA0 → R1 330 Ω → green LED → ground.

## Firmware

EXTI1 is routed to port A (`AFIO_EXTICR1`), enabled for falling edges
(`EXTI_FTSR`, `EXTI_IMR`) and in the NVIC (IRQ 7). The handler clears the
pending bit, counts the edge and toggles the LED. The main loop prints
`edges: N, LED on/off` on USART1 after every interrupt.

## Steps

1. Run the project (F5). Open the Signal Analyzer: Scope tab with V(BTN) at
   1 ms/div, trigger on V(BTN), falling edge, level 1.5 V, mode Normal; and
   the **UART** tab.
2. With SW2 open, click and hold SW1, then release. Watch the UART lines.
3. Stop the scope display (Stop display) and zoom into the edge: the bounces.
4. Close SW2 (click it), wait a moment, then press and release SW1 again.
5. Turn on **Time stamps** in the UART tab and compare the times of the
   edges with the scope.

## Expected observations

- Without the filter one press counts several edges: 5 falling edges while
  pressing and 4 more while releasing with the simulated bounce, so the LED
  may end up on or off by chance.
- The bounces are 0.2–0.5 ms pulses during about 3 ms after the press.
- With the filter V(BTN) falls at once when pressed (C1 discharges through
  the contact) and rises slowly on release (about 8 ms to the 1.83 V input
  threshold): exactly one edge per press.
- Closing SW2 connects the discharged C1: the input falls once by itself;
  that edge is counted too.

## Questions

1. Why does the RC filter remove the bounces on release, although the
   contact still bounces?
2. With τ = 10 ms, how fast can someone press the button and still be
   counted every time?
3. Rewrite the firmware to debounce in software (ignore edges for 20 ms) and
   remove the capacitor. Which approach costs what?

## Limits of the simulation

The bounce pattern is fixed (the same for every press) so that runs are
reproducible; real contacts bounce differently each time. Circuit changes
reach the firmware within 0.1 ms (one coupling window of the emulator,
ADR 0090); input thresholds are 1.16 V / 1.83 V.
