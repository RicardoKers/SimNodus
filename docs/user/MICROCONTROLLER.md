# Microcontroller: what is simulated

SimNodus runs the firmware of the **Blue Pill (STM32F103C8)** in its own STM32
emulator next to the circuit. This page lists what the simulated chip supports and
where it differs from a real board (MCU-09). Release 0.2.

## Engines

Firmware runs in SimNodus's own STM32 emulator (ADR 0089, 0090): it starts at
once, gives pin changes their exact time and avoids the Renode differences
listed below. **Simulate > Use Renode for Microcontrollers (Legacy)** runs it
in Renode 1.16.1 instead, which takes several seconds to start; Renode gets no
new features, so the timers, ADC and later additions work only in the
emulator. The choice applies to the next run and is remembered. With the emulator,
circuit changes reach the firmware within 0.1 ms (a coupling window of
100 µs instead of the 1 ms quantum), and two runs with the same circuit give
the same firmware timing.

## Supported

| Part | Status | Notes |
|---|---|---|
| Cortex-M3 core, 64 KiB flash, 20 KiB SRAM | yes | ELF files built for the STM32F103C8 (STM32CubeIDE, arm-none-eabi-gcc) |
| Clock (RCC) | yes | Starts on HSI 8 MHz. HSE (the board's 8 MHz crystal), PLL, LSE and LSI report ready as soon as they are switched on, and SYSCLK follows SW, so the `SystemClock_Config()` that STM32CubeIDE generates runs (72 MHz with PLL x9). The CPU runs one instruction per HCLK cycle and SysTick counts at HCLK; RESET returns to 8 MHz. Example 12 uses the HAL at 72 MHz |
| Flash access control, PWR | registers only | `FLASH_ACR` (wait states) and `PWR_CR` keep what is written, so HAL can check them; flash programming and low-power modes are not simulated |
| SysTick | yes | Counts at HCLK; slightly fast (RVR instead of RVR + 1 cycles: 125 ppm at 8 MHz, 14 ppm at 72 MHz) |
| GPIO ports A, B, C | yes | Modes from CRL/CRH, ODR/BSRR/BRR/IDR; push-pull, open-drain, inputs with pull-up/pull-down |
| GPIO outputs into the circuit | yes | 3.3 V or 0 V behind 40 Ω; open drain only pulls low |
| GPIO inputs from the circuit | yes | Schmitt trigger: 1 above 1.83 V, 0 below 1.16 V; floating inputs read 0 |
| EXTI 0–15 and NVIC | yes | Lines reach IRQ 6–10, 23 (5–9) and 40 (10–15) as on the chip |
| USART1–3 | yes | Text in the Signal Analyzer's UART tab |
| DMA1 (emulator only) | yes | Seven channels: peripheral and memory addresses, increments, 8/16/32-bit sizes, circular mode, memory-to-memory, half-transfer, transfer-complete and transfer-error flags and interrupts. Requests from ADC1 (channel 1) and USART1–3 TX/RX, so `HAL_ADC_Start_DMA`, `HAL_UART_Transmit_DMA` and `HAL_UART_Receive_DMA` work. A transfer takes no time. Example 15 |
| ADC1 and ADC2 (emulator only) | yes | 12 bits over 0–3.3 V on IN0–IN9 (PA0–PA7, PB0, PB1), the temperature sensor (1.43 V, as at 25 °C) and VREFINT (1.20 V); single, continuous and scan conversion, injected channels with offsets, per-channel sample times and the conversion time from PCLK2 and ADCPRE, calibration, left/right alignment, analog watchdog, end-of-conversion interrupt. The pin voltage is read from the circuit 0.1–0.2 ms before the sample instant (see below). Example 14 |
| Timers TIM1–TIM4 (emulator only) | yes | Counting up and down with prescaler and auto-reload (with preload), one-pulse mode, TIM1 repetition counter; output compare (toggle, active/inactive on match, forced levels), PWM modes 1 and 2 with CCR preload and polarity, TIM1 main output enable and complementary outputs; input capture of rising or falling edges with the capture prescaler; update and capture/compare interrupts; channel pins with the AFIO remaps. Edges are exact to the timer clock. Example 13 |
| On-board LED (PC13, lit when low) and RESET button | yes | Clickable while running |

## Not yet simulated

| Part | Planned |
|---|---|
| ADC features: conversions started by a timer or EXTI, discontinuous and dual modes; DMA requests of timers, SPI and I2C | Later; the firmware gets a message in the Messages panel when it uses one |
| Timer features: center-aligned counting, encoder and other slave modes, input filters, dead time, break input, timer DMA requests | Later; the firmware gets a message in the Messages panel when it uses one |
| SPI, I2C, CAN, USB, RTC, watchdogs | Not planned yet; the firmware's register accesses have no effect |
| Watchpoints (break when a variable changes) | Later |
| Transient mode with a microcontroller | Not yet: use interactive mode |

## Differences from the real board

- **Timing between firmware and circuit.** The firmware runs at most two
  *microcontroller quanta* (Simulate > Simulation Settings, default 1 ms)
  ahead of the circuit. Output changes reach the circuit at their exact
  firmware time (rounded to Renode's 100 µs time step). Circuit changes
  reach the firmware up to two quanta late, in order and with their
  spacing. The status bar tooltip states these approximations.
- **Clock.** Oscillators and the PLL are ready at once (a real HSE takes
  about 1–2 ms). The CPU executes one instruction per clock cycle: a real
  Cortex-M3 needs extra cycles for taken branches, loads and flash wait
  states, so busy-wait loops run faster than on the board (code timed with
  SysTick or `HAL_Delay()` is right). Peripheral clock enables
  (`RCC_APB2ENR` …) are not checked: a peripheral works even if its clock
  was never switched on, which would fail on the board. The clock security
  system and MCO are not simulated.
- **FreeRTOS start-up.** Renode starts SysTick with a full 24-bit period
  when firmware enables it with the counter at 0, as FreeRTOS does: the first
  tick comes about 2.1 s late at 8 MHz (0.23 s at 72 MHz), then ticks are
  regular. A real chip ticks after one period. SimNodus's own emulator, being
  built (E-07), does not have this difference. STM32CubeIDE projects with
  FreeRTOS also use a timer (TIM) as the HAL time base, which only the
  emulator simulates.
- **Speed with 72 MHz firmware.** At 72 MHz Renode executes nine times more
  instructions per simulated second than at 8 MHz. Firmware that waits in a
  loop (`HAL_Delay()`) ran example 12 at about 0.85x real time on a
  2026 laptop; the status bar shows the actual speed.
- **ADC timing.** The firmware runs up to 0.1 ms ahead of the circuit, so a
  conversion reads the pin voltage the circuit had 0.1–0.2 ms before the
  sample instant (ADR 0092). Slow signals (potentiometers, sensors, filtered
  PWM) give the code of their voltage; a fast waveform shows that delay. Two
  runs with the same circuit give the same codes.
- **Edges in the circuit.** The emulator times every edge exactly. The
  circuit takes time steps of up to 0.1 ms (Simulation Settings, maximum
  step) and receives, for each step, the pin's output averaged over that step
  (ADR 0091). So the average of a PWM is exact at any frequency (example 13:
  0.825 V from 25 % of 3.3 V), but an edge is spread over the step that
  contains it: the scope shows one point between the levels, and signals
  faster than the step (a serial frame's bits) are averaged away there. The
  logic lanes show the exact edges. **Simulate > Simulation Settings > Exact
  microcontroller edges** gives the circuit a time point at every firmware
  edge instead (ADR 0093): square waves stay square at the 100 µs step and RC
  responses match the formulas, but runs are slower (lesson 1: 1.2x instead of
  2.9x real time on a 2026 laptop; a 10 µs step gives the same waveforms at
  about 0.25x). With Renode an edge acts at the next time point.
- **AFIO (Renode only).** With Renode, EXTI line *n* follows pin *n* of
  every port; the port selection in `AFIO_EXTICR` is not simulated, so use
  one pin number per EXTI line. The SimNodus emulator simulates `AFIO_EXTICR`
  and the USART pin remaps.
- **UART.** Bytes move instantly: the baud rate is not simulated and the TX
  pin does not carry the serial waveform. The TX pin (PA9, PA2, PB10) rests
  high (3.3 V) while the USART and its transmitter are enabled, as on the
  board; the scope shows that level, not the bits. Real waveforms are planned
  with SimNodus's own STM32 emulator (E-07).
- **Pin speed and drive.** Edge speed (MODE 10/2/50 MHz) and current limits
  are not simulated; an LED without a resistor draws an unrealistic current.
- **Power.** The board is fed by its 3.3 V regulator (an ideal 3.3 V source
  behind 0.1 Ω); 5 V and VBAT pins are not connected inside.

## Debugging with STM32CubeIDE or GDB

SimNodus's emulator contains a GDB server (SN-180), so the firmware can be
debugged while it drives the circuit: breakpoints, stepping, variables,
registers and memory. While the firmware is halted, **the circuit stands
still too**; when it runs again, both continue from the same instant.

1. In SimNodus, check **Simulate > Debug Firmware with GDB (port 3333)** and
   start the run (F5). The status bar shows *Waiting for the debugger (GDB
   port 3333)*: the firmware waits at its reset handler and the circuit at
   0 s. A second board uses port 3334, and so on.
2. The ELF in the board's properties must be the one the debugger uses
   (STM32CubeIDE: `Debug/<project>.elf`); after rebuilding, click
   **Reload**.
3. **STM32CubeIDE:** Run > Debug Configurations…, select the project's
   *STM32 C/C++ Application* configuration. On the **Debugger** tab choose
   *Connect to remote GDB server*, host `localhost`, port `3333`. On the
   **Startup** tab, in *Load Image and Symbols*, edit the entry and clear
   *Download* (SimNodus has already loaded the firmware; keep *Load
   symbols*). Click **Debug**. Then use breakpoints, Step Over/Into,
   Resume, Suspend and the Variables, Registers and Expressions views as
   with a board. *(These steps were written for STM32CubeIDE 2.1.1 and are
   being confirmed in the owner's test; the GDB route below is verified.)*
4. **arm-none-eabi-gdb** (it comes with STM32CubeIDE, in the GNU Tools for
   STM32 folder): `arm-none-eabi-gdb Debug/project.elf`, then
   `target extended-remote :3333`, `break main`, `continue`, `next`,
   `print variable`, `monitor reset`, `detach`.
5. Stopping the debugger (detach or terminate) lets the firmware run on;
   the run itself continues until you stop it in SimNodus.

Supported: any number of breakpoints, Step, Resume, Suspend (Ctrl-C),
registers (r0–r15, xPSR, MSP, PSP, PRIMASK, CONTROL), memory and
peripheral registers, `monitor reset` and `monitor reset halt`, `load`.
Not yet: watchpoints. Reading a peripheral register in the debugger has the
same side effects as on the chip (reading `USART_DR` clears RXNE). The GDB
server listens on 127.0.0.1 only. After `monitor reset`, gdb may keep
showing the old PC until the next step or continue.

## Building firmware

Any STM32F103C8 project works if it produces an ELF, including STM32CubeIDE
projects with the HAL and its generated `SystemClock_Config()`. Most examples
in `examples/firmware/` use no libraries; `hal_clock` (example 12) and
`hal_adc_dma` (example 15, ADC with DMA, timer PWM, UART with DMA) are written
like STM32CubeIDE projects and use the STM32CubeF1 HAL. All build with
`python tools/build_firmware.py` (finds the compiler of an STM32CubeIDE
installation or `arm-none-eabi-gcc` on PATH, and the STM32CubeF1 package
that STM32CubeIDE downloads to `~/STM32Cube/Repository`). Projects store the ELF path
relative to the project file; **Reload** in Properties picks up a rebuilt ELF.

Technical background: ADR 0082–0088 in `docs/decisions/`.
