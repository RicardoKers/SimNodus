# Microcontroller: what is simulated

SimNodus runs the firmware of the **Blue Pill (STM32F103C8)** in its own STM32
emulator next to the circuit. This page lists what the simulated chip supports and
where it differs from a real board (MCU-09); the [user guide](USER_GUIDE.md)
covers the rest of SimNodus. The **ESP32-C3-DevKitM-1** board is new and
experimental: see [ESP32-C3 board (experimental)](#esp32-c3-board-experimental).
So are the ATmega328P, the [ATmega32](#atmega32-and-atmega32a-experimental) and the [Arduino UNO](#arduino-uno-experimental).

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
expander, BMP280, MPU6050, DS3231, TCS34725, MCP3208, MCP41010, W25Q32, microSD
card, HX711, DS18B20, DHT22, HC-SR04) and the character LCD, the SSD1306 OLED and the
WS2812B strip
(under *Displays*) work together with
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
- The I2C sensors, clock and display (SN-223) answer at their fixed
  addresses: BMP280 at 0x76 (0x77 with SDO high) with its calibration words,
  forced and normal modes, so the usual libraries compute the pressure and
  temperature set in its properties; MPU6050 at 0x68 (0x69 with AD0 high),
  asleep until PWR_MGMT_1 is cleared, accelerations and rotation rates scaled
  by the full-scale settings; DS3231 at 0x68, a clock running with the
  simulation from its *Start time* (`now` or a date), two alarms and the SQW
  square wave or alarm output; TCS34725 at 0x29, counts from the color and
  light in its properties, its gain and integration time; SSD1306 OLED at
  0x3C or 0x3D (a property), whose picture appears on the schematic while
  running. The measured values are properties that can change while
  running. Example 38 shows the OLED with a BMP280 and a DS3231.
- The SPI chips (SN-224) work in modes 0 and 3 with their chip select: the
  MCP3208 converts the circuit's voltage on CH0-CH7 against VREF (12 bits,
  single-ended or differential); the MCP41010 sets a real resistance between
  PA0, PW0 and PB0 (10, 50 or 100 kΩ, 256 steps); the W25Q32 is a 4 MB flash
  with the datasheet's commands and busy times (its contents in the Memory
  Viewer, refreshed at most five times a second); the microSD card answers
  the SPI-mode commands FatFs drivers use, with a size of 1 to 64 MB and an
  optional image file. The HX711 load-cell amplifier reads the bridge's
  voltages through its own two-wire protocol (10 or 80 conversions a
  second). Example 39 uses all but the card.
- The timed one-wire chips (SN-225) follow the edge times the firmware makes
  (microsecond delays, for example with the DWT cycle counter): the DS18B20
  answers resets, ROM commands (several sensors share a bus, each with its
  own code) and conversions of 94 to 750 ms, reading +85 °C before the first;
  the DHT22 or DHT11 sends its 40 bits after the start signal; the HC-SR04
  holds ECHO high for 58 µs per centimetre; the WS2812B strip decodes 0.4 and
  0.8 µs pulses and shows its colors on the schematic. Example 40 uses all
  four.
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
  (Simulate > Simulation Settings, on by default), each instruction takes the cycles of
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

## ESP32-C3 board (experimental)

The **ESP32-C3-DevKitM-1** part (Microcontrollers) runs ESP32-C3 firmware in
SimNodus's own RISC-V emulator (ADR 0106). It is **experimental**: the same
firmware has run unmodified (ESP-IDF examples, an Arduino sketch, the official
MicroPython image), but it has not yet been compared with a real board, so
small differences may remain.

- **Firmware**: a flash image (`.bin`) written from address 0, the way
  `esptool write-flash 0x0` writes it: ESP-IDF's `idf.py merge-bin` output,
  the Arduino IDE's `.merged.bin` (Sketch > Export Compiled Binary), or
  MicroPython's `ESP32_GENERIC_C3` firmware `.bin`. An application image
  also works (ESP-IDF's or Espressif-IDE's `build/app.bin`, the Arduino
  IDE's `.ino.bin`) when the build's `flash_args` file is beside it, as the
  build leaves it: SimNodus places the bootloader, the partition table and
  the application at their addresses, as `esptool write-flash @flash_args`
  does. ELF files are not accepted.
  Example 45 blinks an LED on GPIO8 with ESP-IDF's blink example.
- **ROM**: the chip's ROM is rebuilt at start from Espressif's ROM file,
  which comes with SimNodus (`roms/esp32c3_rev3_rom.elf`, Apache-2.0).
- **Pins**: GPIO0-GPIO10 and GPIO18-GPIO21, as on the board's headers; EN,
  3V3, 5V and G pins. Outputs drive 3.3 V or 0 V behind 33 Ω; inputs read 1
  above 2.475 V and 0 below 0.825 V (the datasheet's 0.75 and 0.25 x VDD);
  internal pulls of 45 kΩ. The board's EN and GPIO9 (BOOT) have 10 kΩ
  pull-ups. The RGB LED (a WS2812 on GPIO8) is not drawn.
- **Serial ports** in the terminal: **UART0** (the board's USB connector;
  GPIO21 TX, GPIO20 RX; ESP-IDF's and Arduino's log) and **USB Serial/JTAG**
  (the chip's own USB on GPIO18/GPIO19; MicroPython's `>>>` prompt). UART0
  and UART1 send and receive at the baud rate, bit by bit on their pins, so
  the boot log takes as long as on the board, and the Serial Port part or
  another board wired to GPIO21/GPIO20 talks to them. See
  [The board's USB to the computer](#the-boards-usb-to-the-computer) for
  the Arduino IDE, Thonny, mpremote and esptool.
- **Simulated**: the RV32IMC core, 4 MB flash, SRAM, caches and flash
  mapping, interrupts, system timer, timer groups and watchdogs, RTC, light
  and deep sleep, GPIO with interrupts, UART0/UART1, USB Serial/JTAG, LEDC
  (PWM), the ADC on GPIO0-GPIO4 (approximate: linear, no noise), SHA,
  random numbers and eFuse (MAC 34:85:18:00:C3:01).
- **ADC**: one-shot readings and continuous conversions with DMA (ESP-IDF's
  `adc_continuous` driver, up to 83 kHz), each taking the circuit's voltage
  at its own time, so a waveform is sampled as on the board. Calibrated
  readings are within about 2 % (1.7 % low near 2.4 V with 12 dB). Example
  49 samples a 50 Hz sine and a potentiometer at 20 kHz.
- **Temperature sensor**: it measures the board's **Chip temperature**
  property (25 °C by default; change it while running). ESP-IDF's
  `temperature_sensor` driver, MicroPython's `esp32.mcu_temperature()` and
  Arduino's `temperatureRead()` read it; ESP-IDF rounds down to whole
  degrees, so a reading may be up to 1 °C low.
- **I2C**: I2C0 as a master on any GPIO through the GPIO matrix, with
  SimNodus's I2C chips (the same ones as with the Blue Pill) at the pin level:
  ESP-IDF's I2C driver, Arduino's `Wire` and MicroPython's `machine.I2C`
  work unchanged, with clock stretching and a missing chip reported as a
  NACK. The bus needs pull-ups: resistors in the schematic, or the pins'
  internal 45 kΩ pull-ups, which the drivers switch on. Example 46 reads an
  LM75 and writes a 24C02. Slave mode is not simulated.
- **SPI**: SPI2 as a master (modes 0-3, up to 80 MHz, MSB or LSB first,
  hardware CS0-CS5, command, address and dummy phases) on any GPIO through
  the GPIO matrix or on its own pins (SCLK GPIO6, MOSI GPIO7, MISO GPIO2,
  CS GPIO10), with or without DMA, with SimNodus's SPI chips at the pin
  level: ESP-IDF's SPI master driver, Arduino's `SPI` and MicroPython's
  `machine.SPI` work unchanged. Example 47 reads an MCP3208 and programs a
  W25Q32. Dual and quad lines and slave mode are not simulated.
- **RMT**: the two transmit channels on any GPIO, with symbols in the RMT
  memory, wrap-around refills, loops and the idle level, so pulse trains
  come out with their exact timing: WS2812B strips light from ESP-IDF's RMT
  driver, Arduino's `rgbLedWrite` and MicroPython's `neopixel`. Example 48
  runs a light along a strip. Receive channels and the carrier are not
  simulated.
- **Not yet**: Wi-Fi and Bluetooth.
- **Debugging** with `riscv32-esp-elf-gdb`: see
  [Debugging with STM32CubeIDE or GDB](#debugging-with-stm32cubeide-or-gdb).
- **Timing**: one instruction per CPU clock cycle (160 MHz with the default
  settings); cache misses and flash waits are not modelled, so code runs
  somewhat faster than on the chip. Timers count at their real rates.
- **Pressing RESET** restarts the chip as the EN button does; the flash keeps
  what the firmware wrote (MicroPython's files), and so does the next run
  (see [MicroPython's files](#micropythons-files)).

### The board's USB to the computer

The board's two serial ports can reach programs on the computer, so the
Arduino IDE (Serial Monitor and Upload), Thonny, mpremote, esptool or a
terminal work with the simulated board as with a real one. In the board's
properties, **UART0 to the computer** is the DevKit's USB connector (UART0
behind its USB-serial bridge); **USB Serial/JTAG to the computer** is the
chip's own USB (MicroPython's REPL). Each takes one of:

- **A serial port** of the computer: one end of a pair of virtual ports
  (com0com, or a similar program, creates two ports wired to each other, say
  COM10 and COM11). Choose COM10 in SimNodus and COM11 in the Arduino IDE or
  Thonny. Both programs only list COM ports, so they need the pair.
- **tcp:4000** (any free port number): SimNodus listens on this computer
  only. Programs built on pyserial open it as `rfc2217://localhost:4000`:
  `esptool --port rfc2217://localhost:4000 ...`, and mpremote; no driver is
  needed. A raw TCP terminal (`socket://localhost:4000`, PuTTY's Raw) gets the
  bytes too.
- **Empty**: no link (the terminal only).

The link opens when the run starts; the Messages panel says where each port
is linked, or why it could not be. The terminal keeps showing what the
board sends.

**DTR and RTS** work as on the board. On UART0 they reach EN and GPIO9
through the DevKit's two transistors, so esptool's and the Arduino IDE's
automatic reset puts the chip in download mode, and the ROM's own loader
writes the flash (Upload in the Arduino IDE, `esptool write-flash`). On the
USB Serial/JTAG port they follow the chip's reset logic (esptool's
`--before usb_reset`, or the same sequence as on UART0). The Messages panel
says when new firmware is being written; when the run stops, it is saved
as `U1-flash.bin` (the board's designator) beside the project and becomes
the board's firmware, as a real board keeps what was uploaded; the project
must have been saved once.

Which port an Arduino sketch's `Serial` uses is the sketch's choice, as on a
real board: UART0 by default, the USB Serial/JTAG port with **Tools > USB
CDC On Boot: Enabled** in the Arduino IDE (`Serial0` is then UART0).
MicroPython's REPL is on the USB Serial/JTAG port.

Uploads take the simulated time they need: at 921600 baud and real-time
speed, esptool writes 200 KB in about 3 seconds. Thonny and mpremote may
reset the board when they connect, as they do with a real one.

### MicroPython's files

MicroPython keeps its files (`main.py`, `boot.py`, modules, data) in a file
system in the flash. Copy them to the board and back as with a real one:
in Thonny, **View > Files** with the board's port chosen, then **Upload to /**
or **Download**; with mpremote, `mpremote connect rfc2217://localhost:4000
fs cp main.py :main.py` (and `fs cp :data.txt .` for the way back).

Whatever the firmware writes to the flash (MicroPython's files, ESP-IDF's
NVS, Arduino's `Preferences`) is kept as uploads are: when the run stops,
the flash is saved as `U1-flash.bin` beside the project and becomes the
board's firmware, so the next run starts with the files and runs `main.py`
at boot. The project must have been saved once: an example opened from
**File > Examples** keeps nothing until it is saved under a name of its own.
To start again from a clean image, choose the original `.bin` in the
board's **Firmware** property again (or undo the change), or erase it with
`esptool erase-flash` and write the image again.

## ATmega8 and ATmega8A

The **ATmega8-PU** and **ATmega8A-PU** parts (Microcontrollers) are the bare
AVR chips in PDIP-28, as on a breadboard. They run in SimNodus's own AVR
emulator (experiment E-10, ADR 0108).
- Every instruction has been checked against the AVR Instruction Set
  Manual, and the peripherals against the datasheet.
- Examples 50, 51, 52 and 54 behaved on a real ATmega8 as in the
  simulation: serial port, PWM, crystal clock, EEPROM and sleep.
- The ADC, the analog comparator and the brown-out levels are approximate.
  They have not been measured on a chip.

- **Firmware**: an Intel HEX file, as written by Microchip Studio, MPLAB X,
  `avr-objcopy` or the Arduino IDE's Sketch > Export Compiled Binary. An
  avr-gcc ELF file also works. Its fuses (the `FUSES` macro) and its EEPROM
  data apply as a production programmer writes them. When a run starts, an
  ELF's fuses that differ from the part's become the part's properties, in
  one step that Undo takes back, and the messages say so.
- **Power**: wire VCC, AVCC and both GND pins. The chip reads its supply
  from the circuit:
  - Below about 1.4 V on VCC it is off and says so; when VCC returns, it
    starts with a power-on reset.
  - With **BODEN** it is held in reset below the brown-out level (2.6 V,
    or 3.8 V with BODLEVEL programmed), and starts again with BORF.
  - Without BODEN, a supply below the datasheet's minimum for the clock
    (2.7 V; 4.5 V above 8 MHz or for the ATmega8) gives a warning.
  - The pins drive the circuit as at 5 V whatever VCC is.
- **Fuses**: the **Low fuse**, **High fuse** and **Lock bits** properties,
  in hex, with programmed bits as 0. They take effect when the run starts,
  as at power-up. **Fuses...** below them shows the same bits field by field
  and keeps both views in step: the clock source and its start-up time,
  brown-out, CKOPT, RSTDISBL, WDTON, EESAVE, the boot section and the lock
  modes. A checked box is a programmed bit. **Factory values** sets E1, D9
  and FF again.
  - The factory values (E1, D9) select the internal RC oscillator at 1 MHz
    with a 65 ms start-up, as on a new chip. Code built for 16 MHz runs 16
    times slower until the fuses change.
  - For a crystal, set the low fuse to EF and the high fuse to C9 (CKOPT),
    and place a **Crystal** part (Passive) between XTAL1 (PB6) and XTAL2
    (PB7). The crystal's frequency is the chip's clock; its oscillation is
    not simulated.
  - With a crystal selected and none connected, the chip does not start,
    and the log says why.
  - RSTDISBL makes PC6 an I/O pin; otherwise it is RESET, with its pull-up,
    and taking it low resets the chip. EESAVE, WDTON, BOOTSZ and BOOTRST
    act as on the chip.
- **What runs**:
  - the ports, INT0 and INT1;
  - Timer0, Timer1 and Timer2 in every mode, exact in cycles (PWM on OC1A,
    OC1B and OC2, input capture);
  - the USART, bit by bit on PD1 and PD0, in the terminal or through
    **USART to the computer**;
  - the EEPROM, the watchdog, the resets, and self-programming (SPM);
  - the ADC: ADC0-ADC5 on PC0-PC5, the bandgap and GND; the AVCC, AREF or
    internal 2.56 V reference, with AVCC and AREF taken from the circuit;
    single and free-running conversions with their timing (13 ADC clocks,
    25 for the first). Each reads the circuit's voltage at its sampling
    instant;
  - the analog comparator: AIN0 or the bandgap against AIN1 or an ADC
    input, with its interrupt and Timer1's input capture. It samples the
    circuit every 10 µs, so an edge shorter than that can be missed;
  - SPI as a master, bit by bit on SCK (PB5), MOSI (PB3) and MISO (PB4) in
    the four modes, up to F_CPU / 2. SS (PB2) taken low as an input ends
    master mode, as on the chip;
  - TWI (I2C) as a master on SDA (PC4) and SCL (PC5), open drain, with the
    TWSR status codes and clock stretching. It talks to the I2C chips of
    the library, with pull-ups in the circuit;
  - the sleep modes:
    - Idle, ADC Noise Reduction, Power-down, Power-save and Standby, with
      the datasheet's wake-up sources: from Power-down only a level
      interrupt on INT0/INT1 or the watchdog wakes the chip;
    - the timers stand still while the I/O clock stops;
    - waking from Power-down takes the oscillator's start-up time;
  - Timer2 asynchronous (AS2) on a 32.768 kHz **Crystal** between TOSC1
    (PB6) and TOSC2 (PB7), with the main clock on the internal RC. It keeps
    running in Power-save, and its update-busy flags work as on the chip.
- **Not yet**: SPI and TWI as slaves. The log says once when the firmware
  uses one.
- **EEPROM**: what the firmware writes to it is kept beside a saved project
  as `<designator>-eeprom.bin`, which becomes the chip's **EEPROM** property.
  The next run starts with it, as the chip keeps its EEPROM when it is
  powered off.
  - With no kept EEPROM, the chip starts erased, or with the ELF's EEPROM
    data.
  - Clear the property to start erased again.
  - An example from **File > Examples** is a copy without a file: what a
    run writes to the EEPROM is kept when the project is saved (the
    messages say so after the run).
  - **View...** beside the property opens the **Memory Viewer** on the
    chip's EEPROM: the bytes as the running chip has them, or as the last
    run left them. **Load...** there (with the simulation stopped) makes a
    `.bin` or `.hex` file the chip's EEPROM.
- **Examples**: 50 (the USART at 9600 baud with a counter kept in the
  EEPROM), 51 (a blink at 16 MHz with a crystal) and 52 (a potentiometer on
  the ADC setting a PWM, kept in step by a 1 ms timer tick) and 53 (an
  LM75 and a 24C02 over TWI, a 74HC595 over SPI) and 54 (a real-time clock
  on a watch crystal, sleeping in Power-save). Their firmware sources
  are in `examples/firmware/avr_*`.

## ATmega328 and ATmega328P (experimental)

The **ATmega328P-PU** (the Arduino UNO's chip) and **ATmega328-PU** parts
(Microcontrollers) have the ATmega8's pinout and run in the same emulator
(SN-260). They are **experimental** until they are compared with a real
chip (ADR 0108). The two differ only in their signature (1E 95 0F and
1E 95 14) and the 328P's BODS. Everything in the ATmega8's section above
holds, with these differences:

- **Memories**: 32 KB of flash, 2 KB of SRAM, 1 KB of EEPROM. The
  interrupt vectors take two words (JMP).
- **Fuses**: a third byte, the **Extended fuse**, holds BODLEVEL. The
  **Fuses...** dialog shows the 328's fields.
  - The factory values (62, D9, FF) select the internal 8 MHz RC
    oscillator divided by 8 (CKDIV8): the chip starts at 1 MHz.
    Firmware may change the division at run time through CLKPR (avr-libc's
    `clock_prescale_set`), and the simulation's clock follows.
  - The Arduino UNO's fuses are FF, DE and FD: a 16 MHz crystal (place a
    **Crystal** between PB6 and PB7), a 256-word boot section with the
    reset vector in it, brown-out at 2.7 V.
  - The low-power crystal oscillator warns when the crystal is above the
    range CKSEL3:1 selects. CKOUT and DWEN are noted in the log.
- **Brown-out**: BODLEVEL 1.8, 2.7 or 4.3 V, from the extended fuse.
- **What runs**, on top of the ATmega8's:
  - Timer0 and Timer2 with two compare outputs each (OC0A PD6, OC0B PD5,
    OC2A PB3, OC2B PD3) in all eight waveform modes; Timer1's TCCR1C;
  - the pin change interrupts PCINT0-23 on ports B, C and D. They wake
    the chip from every sleep mode;
  - INT0 and INT1 through EICRA and EIMSK; writing 1 to a PINx bit toggles
    the pin;
  - the watchdog's interrupt mode (WDIE), its 128 kHz oscillator
    (16 ms to 8 s), and the reset that WDRF keeps on until the firmware
    clears MCUSR, as on the chip;
  - the ADC's 1.1 V reference, the temperature sensor (ADC8, read as at
    25 °C: about 352; real chips need calibration) and auto triggering
    from ADTS (free running, the comparator, INT0, Timer0 and Timer1
    events);
  - USART0's own UCSR0C and UBRR0H, the EEPROM's erase-only and write-only
    modes (EEPM), DIDR0 and DIDR1, GPIOR0-2, extended standby sleep;
  - signature and fuse reads through SPM's SIGRD and BLBSET.
- **Not yet**: the USART's SPI master mode, PRR stopping a module (it is
  accepted; the module keeps running), Timer2's external clock (EXCLK),
  the Arduino UNO board and its bootloader (SN-261), GDB (SN-262).
- **Examples**: 55 (serial at 38400 baud after CLKPR, the temperature
  sensor, a counter in the EEPROM), 56 (the four 8-bit PWM outputs and an
  ADC triggered by Timer0's overflow) and 57 (power-down woken by the
  watchdog, a pin change and INT0). Their firmware sources are in
  `examples/firmware/avr328_*`.

## ATmega32 and ATmega32A (experimental)

The **ATmega32-PU** and **ATmega32A-PU** parts (Microcontrollers) are the
PDIP-40 chips (SN-263), in the same emulator. They are experimental until
they are compared with a real chip. They follow the ATmega8's section above
(the same register layout, fuses and clock sources), with these differences:

- **Pins**: four ports, A to D (PA0-PA7 are ADC0-ADC7). **RESET**, **XTAL1**
  and **XTAL2** are pins of their own; a **Crystal** goes between XTAL1 and
  XTAL2, and Timer2's 32.768 kHz crystal between TOSC1 (PC6) and TOSC2
  (PC7).
- **JTAG**: the factory high fuse (99) has **JTAGEN** programmed, so PC2-PC5
  are the JTAG pins and do not follow DDRC and PORTC, as on a new chip. The
  firmware frees them by writing JTD in MCUCSR twice within four cycles
  (`MCUCSR = _BV(JTD); MCUCSR = _BV(JTD);`), or the high fuse D9 does. The
  log says so when firmware writes those pins while JTAG holds them.
- **Memories**: 32 KB of flash, 2 KB of SRAM, 1 KB of EEPROM; two-word
  vectors.
- **What runs**, on top of the ATmega8's:
  - Timer0 with a compare output on OC0 (PB3) in its four modes; Timer1's
    outputs on OC1A (PD5) and OC1B (PD4), input capture on ICP1 (PD6);
    Timer2's OC2 on PD7;
  - INT2 on PB2 (an edge chosen by ISC2; it wakes the chip from every sleep
    mode);
  - the ADC's eight inputs, the differential channels with a gain of 1, 10
    or 200 (the datasheet's ideal values: offset and gain errors are not
    simulated), the 1.22 V bandgap, auto triggering from SFIOR's ADTS;
  - SPI on PB4-PB7, TWI on PC0 (SCL) and PC1 (SDA), the comparator on PB2
    (AIN0) and PB3 (AIN1); extended standby sleep.
- **Not yet**: JTAG and on-chip debugging themselves (OCDEN has no effect).
- **Example**: 59 (JTD and a count on PC2-PC5, a PWM on OC0 from the
  potentiometer, INT2, the differential ADC). Its firmware source is in
  `examples/firmware/avr32_demo`.

## Arduino UNO (experimental)

The **Arduino UNO R3** part (Microcontrollers) is the board as it leaves the
factory (SN-261): an ATmega328P (see the section above) at 16 MHz with
**Optiboot** in its boot section and the Arduino fuses (FF, DE, FD; lock
0F), so the Arduino IDE and avrdude upload to it unchanged. Its pins carry
the header names: **D0**-**D13**, **A0**-**A5**, **RESET**, **AREF**, and
the board's **5V**, **3V3**, **IOREF**, **VIN** and **GND**. It is
experimental until it is compared with a real board.

- **Power**: the board is fed by its USB connector. **5V** and **3V3** are
  outputs for the circuit; wire a **GND** to the circuit's ground.
- **The L LED** on D13 lights on the board, and **RESET** on the board
  resets it (Optiboot then waits about a second for an upload, as on a
  real board).
- **Firmware**: a sketch's `.hex` (the Arduino IDE's **Sketch > Export
  Compiled Binary**, with or without the bootloader) or an avr-gcc `.elf`.
  Empty: the board has only its bootloader and waits for an upload.
- **USB to the computer**: the USART (D1 TX, D0 RX) through the board's
  USB connector, as **UART0 to the computer** on the ESP32-C3 above: a
  virtual port pair (com0com or similar) or `tcp:PORT`. **DTR** resets the
  board through its 100 nF capacitor, as the Arduino IDE does before an
  upload. Some virtual port pairs (Eterlogic's, with the avrdude of the
  Arduino IDE) do not pass that DTR on, so the board also restarts into
  Optiboot when avrdude asks a running sketch for the bootloader twice
  (its first "not in sync" attempts are normal); the Messages panel says
  so:
  1. Set **USB to the computer** to one end of the pair (COM10) and start
     the run.
  2. In the Arduino IDE choose **Tools > Board > Arduino UNO** and the
     other end (COM11), then **Upload**. avrdude resets the board and
     Optiboot writes the sketch; the **Serial Monitor** (at the sketch's
     baud rate) works on the same port.
  3. When the run stops, the flash is saved as `U1-flash.bin` beside the
     project and becomes the board's firmware, and what the sketch wrote to
     the EEPROM as `U1-eeprom.bin`. The project must have been saved once.
- **Not simulated**: the USB bridge (the ATmega16U2) itself, the 1 kΩ
  resistors from it to D0 and D1, the TX and RX LEDs, VIN's regulator.
- **Example**: 58 (the serial example of section *ATmega328 and
  ATmega328P* built for 16 MHz; `l` toggles the L LED).

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
   and Expressions views as with a board. Tested with STM32CubeIDE 2.1.1.
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

**ESP32-C3** (SN-249): the same menu works with `riscv32-esp-elf-gdb`
(ESP-IDF installs it with its tools). The board keeps its **Firmware**
(the `.bin` image, or what esptool uploaded); gdb reads only the
application's ELF for its symbols, and `load` is not supported. The run
starts with the firmware halted at the ROM's reset vector (0x40000000), so
`continue` goes through the ROM and the bootloader into the application:

```text
riscv32-esp-elf-gdb build/app.elf
(gdb) target remote :3333
(gdb) break app_main
(gdb) continue
(gdb) bt
(gdb) next
(gdb) watch s_led_state
(gdb) detach
```

Registers: x0-x31 (by their ABI names: ra, sp, a0, ...), pc and the
machine CSRs mstatus, misa, mie, mtvec, mscratch, mepc, mcause, mtval and
mip. Breakpoints, watchpoints, stepping, Ctrl-C, `monitor reset halt` and
detach work as on the Blue Pill; with a debugger attached, an `ebreak` in
the firmware halts it instead of raising an exception. Tested with
`riscv32-esp-elf-gdb` 16.3 from ESP-IDF v5.5.2 on ESP-IDF's blink example
(breakpoints, a backtrace through FreeRTOS, `next`, a watchpoint, detach).

The halt at 0x40000000 is in the ROM, which has no debug information in the
application's ELF: an IDE shows "no debug information available" there, and
gdb shows `?? ()`. That is expected; set a breakpoint at `app_main` and
continue. For names in the ROM, load its symbols too: `add-symbol-file
roms/esp32c3_rev3_rom.elf` (the file is in the SimNodus folder).

**Espressif-IDE**: in a *GDB Hardware Debugging* configuration, choose the
project's ELF, `riscv32-esp-elf-gdb` as the debugger and the remote target
`localhost`, port `3333`; on the **Startup** tab clear *Load image* and set
a breakpoint at `app_main`. Tested with Espressif-IDE and ESP-IDF v6.1's
blink example. A new connection replaces the one that is attached, so relaunching the
debug session while the previous one is still connected works.

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
