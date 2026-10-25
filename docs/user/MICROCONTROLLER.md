# Microcontroller: what is simulated

SimNodus runs the firmware of the **Blue Pill (STM32F103C8)** in its own STM32
emulator next to the circuit. This page lists what the simulated chip supports and
where it differs from a real board (MCU-09); the [user guide](USER_GUIDE.md)
covers the rest of SimNodus.

## Engine

Firmware runs in SimNodus's own STM32 emulator (ADR 0089, 0090, 0094): it
starts at once and gives pin changes their exact time. Circuit changes reach
the firmware within 0.1 ms (a coupling window of 100 µs), and two runs with
the same circuit give the same firmware timing.

## Supported

| Part | Status | Notes |
|---|---|---|
| Cortex-M3 core, 64 KiB flash, 20 KiB SRAM | yes | ELF files built for the STM32F103C8 (STM32CubeIDE, arm-none-eabi-gcc) |
| Clock (RCC) | yes | Starts on HSI 8 MHz. HSE (the board's 8 MHz crystal), PLL, LSE and LSI report ready as soon as they are switched on, and SYSCLK follows SW, so the `SystemClock_Config()` that STM32CubeIDE generates runs (72 MHz with PLL x9). Instructions take the Cortex-M3's cycles at HCLK (see *Instruction timing* below) and SysTick counts at HCLK; RESET returns to 8 MHz. Example 12 uses the HAL at 72 MHz |
| Flash access control, PWR | registers only | `FLASH_ACR` (wait states) and `PWR_CR` keep what is written, so HAL can check them; flash programming and low-power modes are not simulated |
| SysTick | yes | Counts at HCLK; a period of RVR + 1 cycles, as on the chip |
| GPIO ports A, B, C | yes | Modes from CRL/CRH, ODR/BSRR/BRR/IDR; push-pull, open-drain, inputs with pull-up/pull-down |
| GPIO outputs into the circuit | yes | 3.3 V or 0 V behind 40 Ω; open drain only pulls low |
| GPIO inputs from the circuit | yes | Schmitt trigger: 1 above 1.83 V, 0 below 1.16 V; floating inputs read 0 |
| EXTI 0–15 and NVIC | yes | Lines reach IRQ 6–10, 23 (5–9) and 40 (10–15) as on the chip |
| USART1–3 | yes | Text in the Signal Analyzer's UART tab. Each byte goes out on the TX pin bit by bit at the baud rate (start, 8 or 9 data bits with parity, 0.5–2 stop bits); TXE, TC and the byte's time follow the frame, so serial output takes as long as on the board. A board's TX wired to another board's RX passes the bytes on |
| DMA1 | yes | Seven channels: peripheral and memory addresses, increments, 8/16/32-bit sizes, circular mode, memory-to-memory, half-transfer, transfer-complete and transfer-error flags and interrupts. Requests from ADC1 (channel 1) and USART1–3 TX/RX, so `HAL_ADC_Start_DMA`, `HAL_UART_Transmit_DMA` and `HAL_UART_Receive_DMA` work. A transfer takes no time. Example 15 |
| ADC1 and ADC2 | yes | 12 bits over 0–3.3 V on IN0–IN9 (PA0–PA7, PB0, PB1), the temperature sensor (1.43 V, as at 25 °C) and VREFINT (1.20 V); single, continuous and scan conversion, injected channels with offsets, per-channel sample times and the conversion time from PCLK2 and ADCPRE, calibration, left/right alignment, analog watchdog, end-of-conversion interrupt. The pin voltage is read from the circuit 0.1–0.2 ms before the sample instant (see below). Example 14 |
| Timers TIM1–TIM4 | yes | Counting up and down with prescaler and auto-reload (with preload), one-pulse mode, TIM1 repetition counter; output compare (toggle, active/inactive on match, forced levels), PWM modes 1 and 2 with CCR preload and polarity, TIM1 main output enable and complementary outputs; input capture of rising or falling edges with the capture prescaler; update and capture/compare interrupts; channel pins with the AFIO remaps. Edges are exact to the timer clock. Example 13 |
| SPI1, SPI2 | yes | Master mode: 8 or 16 bits, MSB or LSB first, the four clock modes, PCLK / 2..256 with every SCK edge at its exact time; MOSI and MISO on the pins, so chips on the board's nets answer within the transfer; back-to-back frames, TXE, RXNE, BSY, OVR, interrupts, DMA (SPI1 on channels 2/3, SPI2 on 4/5), NSS output (SSOE), SPI1 remap. `HAL_SPI_Transmit` works unchanged. Example 26 |
| I2C1, I2C2 | yes | Master mode, 7-bit addresses, standard (100 kHz) and fast mode with both duty cycles; SCL and SDA are open drain and need pull-up resistors, as on a board; the clock waits while a slave holds SCL low, and stays low without a pull-up (a message says so); START, repeated START, STOP, acknowledge and AF, TXE/RXNE/BTF with ACK and POS as `HAL_I2C_Master_Transmit/Receive`, `HAL_I2C_Mem_Write/Read` and `HAL_I2C_IsDeviceReady` use them; interrupts, DMA (I2C1 on channels 6/7, I2C2 on 4/5), I2C1 remap. Example 27 |
| CRC | yes | CRC-32 (polynomial 0x04C11DB7) over 32-bit words as `HAL_CRC_Calculate` uses it; IDR; RESET |
| Independent watchdog (IWDG) | yes | Key register, prescaler and reload; the chip resets when it is not reloaded in time (RCC_CSR IWDGRSTF, a message in the Messages panel). Counts at LSI = 40 kHz exactly |
| Window watchdog (WWDG) | yes | 7-bit counter at PCLK1 / 4096 / 2^WDGTB; reset when T6 clears, when refreshed above the window or when T6 is written as 0; early wakeup interrupt at 0x40 |
| RTC and backup registers | yes | Prescaler and 32-bit counter at LSE (32.768 kHz), LSI (40 kHz) or HSE / 128; alarm, second and overflow flags and interrupts, the alarm on EXTI line 17; BKP_DR1–DR10. The backup domain keeps the time and the registers through resets, is cleared at power-on and by BDRST, and needs PWR_CR.DBP for writes, as on the chip. `HAL_RTC_*` and `HAL_RTCEx_BKUP*` work unchanged. Example 24 |
| On-board LED (PC13, lit when low) and RESET button | yes | Clickable while running |

## Chips for microcontrollers

Parts in the *Chips for microcontrollers* category (74HC595 shift register,
MAX7219 display driver, 24C02 EEPROM, LM75 temperature sensor, PCF8574 I/O
expander) and the character LCD (HD44780, under *Displays*) work together with
a board (ADR 0102): their logic runs in step
with the firmware, so the nets between the board and the chip follow every
edge at once, and the chip's outputs drive the circuit like the board's pins
(LEDs light, the scope and the Logic tab show them, as `U2.Q0` lanes).

- Wire the chip's supply pin (VCC) to a DC source or the board's 3V3 and its
  GND to ground; without them it does nothing and the Messages panel says so.
- A chip that shares a net with a board pin, directly or through another
  chip, runs with that board. A chip on no board pin runs on its own (a ROM
  addressed by a counter, a 74HC595 clocked by a push button): its inputs come
  from the circuit up to 0.1 ms late, so clocks up to a few kHz work.
- Inputs tied to ground or to the supply are read as constants; an open input
  reads low (a message names it). A chip pin on a net without a board pin (a
  push button or an LED on a PCF8574 pin) is read from the circuit, like a
  board input, whenever no output drives the net.
- I2C chips answer at their address with A0-A2 as the low bits; the 24C02
  stores a written page at STOP and then does not acknowledge for its 5 ms
  write cycle (poll it with `HAL_I2C_IsDeviceReady`); its *Contents file*
  property (`.bin` or Intel `.hex`) sets what it holds at the start of each
  run, otherwise it starts erased (0xFF); the LM75 reports the
  temperature set in its properties, which can be changed while running; the PCF8574's pins are outputs and inputs
  at once (a 1 is a weak pull-up).
- The parallel ROM and RAM (*Memories*) have A0-A15, D0-D7 and active-low CE,
  OE and (RAM) WE; their size, 16 bytes to 64 KB, is a property, and the
  address pins beyond it are ignored. With CE and OE low they drive D0-D7;
  the RAM stores D0-D7 when WE rises with CE low. The ROM's contents come
  from its *Contents file* (`.bin` or Intel `.hex`, else 0xFF); the RAM starts
  with zeros or its file. Timing is ideal (no access time).
- The character LCD (HD44780) takes RS, RW, E and D0-D7 or D4-D7 from the
  board's pins or from a PCF8574 (as on the I2C backpacks); its rows and
  columns are properties, and its text appears on the schematic while running.
  Instructions take 37 us (clear and home 1.52 ms); a byte written while the
  LCD is busy is ignored, as on the chip, with a message. Custom characters
  (CGRAM) show as blocks.
- On a net, a driven low wins, then a driven high, then a pull-up or
  pull-down: the board pin's internal one or a resistor to a supply or ground.
  Two outputs driving opposite levels are reported as a short circuit.
- Levels are digital: a 5 V chip accepts the board's 3.3 V highs, which a
  real chip may not.

### Memory Viewer

**Window > Memory Viewer** (Ctrl+Shift+M), or **View...** next to a memory
chip's *Contents file* (24C02, ROM, RAM), shows the chip's bytes in hexadecimal and as text.
While the simulation runs the table follows the chip and marks the bytes that
just changed; after a run it keeps the last contents. **Load...** puts a `.bin`
or Intel `.hex` file into the running chip, or, when nothing runs, makes it the
chip's contents file. **Save as...** writes what the chip holds to a `.bin` or
`.hex` file, for example to start the next run with what the firmware stored.

Example 32 uses a ROM without a board as the lookup table of a 7-segment
display; in example 33 a Blue Pill writes and reads a parallel RAM.

Example 25 shifts a byte out bit by bit into a 74HC595 that drives eight LEDs.
Example 27 uses the three I2C chips with the HAL; example 28 drives a 16x2 LCD
through a PCF8574 backpack. Example 26 sends MAX7219 words over SPI1 with the HAL; the chip scans an 8x8
LED matrix (a 4-digit 7-segment display works the same way). Multiplexed LEDs
glow with their current averaged over 20 ms, as the eye sees them. With 64
LEDs the circuit is heavy: example 26 runs at about 0.8x real time on a 2026
laptop.

## Not yet simulated

| Part | Planned |
|---|---|
| ADC features: conversions started by a timer or EXTI, discontinuous and dual modes; DMA requests of timers, SPI and I2C | Later; the firmware gets a message in the Messages panel when it uses one |
| Timer features: center-aligned counting, encoder and other slave modes, input filters, dead time, break input, timer DMA requests | Later; the firmware gets a message in the Messages panel when it uses one |
| SPI slave, receive-only and bidirectional modes, CRC, I2S | Not yet; a message says so |
| I2C slave mode, 10-bit addresses, SMBus, PEC, DMA's LAST bit | Not yet; a message says so |
| CAN, USB, RTC tamper pin and calibration output | Not planned yet |
| Transient mode with a microcontroller | Not yet: use interactive mode |

## Differences from the real board

- **Timing between firmware and circuit.** The firmware runs at most one
  coupling window (100 µs) ahead of the circuit. Output changes reach the
  circuit at their exact firmware time. Circuit changes reach the firmware
  less than 0.1 ms late, in order and with their spacing. The status bar
  tooltip states these approximations.
- **Clock.** Oscillators and the PLL are ready at once (a real HSE takes
  about 1–2 ms). Peripheral clock enables
  (`RCC_APB2ENR` …) are not checked: a peripheral works even if its clock
  was never switched on, which would fail on the board. The clock security
  system and MCO are not simulated.
- **Instruction timing.** With *Cortex-M3 instruction timing* on
  (Simulate > Settings, on by default), each instruction takes the cycles of
  the STM32F103's core: 2 for a load, 2 to 12 for a division, 1 + the
  registers for LDM/STM and PUSH/POP, a pipeline refill after a taken branch,
  12 cycles to enter an interrupt and 10 to return, and the flash wait states
  set in `FLASH_ACR` (two at 72 MHz) through the prefetch buffer. This is an
  approximation calibrated on a real Blue Pill with DWT `CYCCNT`: the 30
  measurements of `examples/firmware/cycle_loops` (15 tests at 8 and 72 MHz)
  agree within 5 %, except eight multiplies in a row at 72 MHz (7 % fast).
  Busy-wait loops and computations therefore take about as long as on the
  board. Off: one instruction per cycle, faster than the chip (as in releases
  up to 0.8). Code timed with SysTick, timers or `HAL_Delay()` is right
  either way. DMA and the CPU do not slow each other down.
- **Speed.** At 72 MHz the emulator executes nine times more instructions
  per simulated second than at 8 MHz; the status bar shows the actual speed.
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
  about 0.25x).
- **UART.** The logic lanes show the exact bits on the TX pin. On the scope a
  bit at 115200 baud (8.7 µs) is shorter than the circuit's 0.1 ms step: turn
  on *Exact microcontroller edges* with a 10 µs step to see the frame. The RX
  pin is decoded from the circuit: any source on RX, for
  example another board's TX through a resistor or logic gates, sends frames
  that the USART reads at the bit centers, with the framing (FE) and parity
  (PE) flags. The circuit must show the bits: turn on *Exact microcontroller
  edges* (any baud rate when a board drives the line), or keep a bit at least
  four time steps long (2400 baud or slower at the 0.1 ms step); otherwise
  the Messages panel says that the pin is not decoded. A byte reaches DR about
  0.4 ms after its stop bit (inputs reach the firmware late, by up to one
  coupling window). Bytes also arrive from the terminal, and a board's TX on
  the same net as another board's RX is passed on at the byte level (one frame
  apart) at any step.
- **Pin speed and drive.** Edge speed (MODE 10/2/50 MHz) and current limits
  are not simulated; an LED without a resistor draws an unrealistic current.
- **Watchdogs and RTC.** LSI runs at exactly 40 kHz (a real chip: 30–60 kHz),
  so IWDG timeouts and an LSI-clocked RTC are exact here and vary on boards.
  LSE starts at once (a real crystal needs up to a few seconds). RTC writes
  in configuration mode take effect at once (RTOFF always reads 1). While the
  debugger halts the firmware, the watchdogs and the RTC stand still too
  (time stops), as with DBGMCU's stop bits set.
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
2. The debugger loads the firmware, as with an ST-LINK: the board's
   firmware field may stay empty, and if it holds an ELF, the debugger's
   download replaces it. After a download the chip starts from reset.
3. **STM32CubeIDE:** Run > Debug Configurations…, select the project's
   *STM32 C/C++ Application* configuration. On the **Debugger** tab choose
   *Connect to remote GDB server*, host `localhost`, port `3333`, and click
   **Debug** (keep *Download* checked on the **Startup** tab). Then use
   breakpoints, Step Over/Into, Resume, Suspend and the Variables, Registers
   and Expressions views as with a board. The owner debugged this way from
   STM32CubeIDE 2.1.1 on 2026-10-18.
4. **arm-none-eabi-gdb** (it comes with STM32CubeIDE, in the GNU Tools for
   STM32 folder): `arm-none-eabi-gdb Debug/project.elf`, then
   `target extended-remote :3333`, `break main`, `continue`, `next`,
   `print variable`, `monitor reset`, `detach`.
5. Stopping the debugger (detach or terminate) lets the firmware run on;
   the run itself continues until you stop it in SimNodus.

Supported: any number of breakpoints and watchpoints (break when a variable
is written, read or either: STM32CubeIDE's *Add Watchpoint*, gdb's `watch`,
`rwatch`, `awatch`; the firmware runs slower while one is set), Step, Resume,
Suspend (Ctrl-C),
registers (r0–r15, xPSR, MSP, PSP, PRIMASK, CONTROL), memory and
peripheral registers, `monitor reset` and `monitor reset halt`, `load`.
Reading a peripheral register in the debugger has the
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

Technical background: ADR 0083–0094 in `docs/decisions/`.
