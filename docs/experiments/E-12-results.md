# E-12: STM32F407VET6 on the own emulator — results

Plan and gates: [E-12-plan.md](E-12-plan.md). This report grows with the
work; a gate is marked met only with its numbers here.

Status on 2026-11-13: **G12a met on the host; G12c met on the host, and
G12c.7 by the owner's comparison on the board; G12b met on the host except
what only the board answers** (G12b.5 `f4_float`'s checksum, G12b.7
STM32CubeIDE's debugger, G12b.9 the blink period and the timing
calibration with `f4_timing`). The review of every criterion is in
[Gate review (SN-299)](#gate-review-sn-299) below.

## Prerequisites (SN-292)

**Toolchain.** The GNU tools of STM32CubeIDE 2.1.1
(`gnu-tools-for-stm32.14.3.rel1`): `arm-none-eabi-as` assembled every
encoding used in the tests (`.cpu cortex-m4`, `.fpu fpv4-sp-d16`), so the
encodings are the ones STM32CubeIDE's compiler emits. Two findings from it:
- the assembler refuses `VMOV.32 Dd[x], Rt` for this FPU. The core decodes
  it (ARMv7-M lists it for the FP extension); it is tested with a
  hand-made encoding;
- double-precision instructions are refused, as the chip has none: the core
  answers them with an undefined instruction.

**References.** The core is written from the ARMv7-M Architecture
Reference Manual (DDI 0403E) pseudocode and the Cortex-M4 Technical
Reference Manual's cycle table. **Not done yet:** the documents were not
open while the code was written (no PDF in the session), so the semantics
and the cycle counts come from the manual as known, checked by the tests
below and to be confirmed against the documents' text and the owner's chip
(SN-296). Document revisions are to be recorded here then.

**Oracles.**
- The host's IEEE 754 single precision (MSVC, x86-64 SSE, `<cfenv>`
  rounding modes): used, see G12a.2.
- The Cortex-M3 path for shared instructions: used, see G12a.2.
- QEMU: not used (no Windows build at hand); optional in the plan.
- The owner's chip: waits for the STM32F407 machine (SN-294) and a
  self-checking firmware (`f4_float`, G12b.5).

**Owner's bench** (confirmed 2026-11-11): ST-LINK/V2 (an original and a
clone) and an ST-LINK V3 for SWD with STM32CubeIDE, a USB-serial adapter
and a logic analyzer; the test board has a 25 MHz crystal and a 3.3 V
regulator, and a 32.768 kHz crystal can be added on the breadboard for the
RTC comparisons (G12c.6).

## What was built (SN-292, SN-293)

- `Cpu::set_architecture(CortexM3 | CortexM4F)` in `src/emu/cpu.*`. The
  decoder is shared: DSP and FPU encodings decode on every core, and a
  Cortex-M3 answers them as before (UNDEFINSTR, or NOCP for the
  coprocessor space).
- `src/emu/softfloat32.*`: single precision in integer arithmetic (add,
  subtract, multiply, divide, square root, fused multiply-add, compare,
  conversions to and from integers and fixed point, half precision), with
  ARM's NaN rules, default NaN, flush-to-zero and tininess before
  rounding. The host's `float` is not used by the emulator.
- `src/emu/cpu_m4.*`: the DSP extension (41 instruction forms: saturating
  arithmetic, the parallel add and subtract family with the GE flags,
  `SEL`, extend-and-add, `PKH`, `SSAT16`/`USAT16`, `USAD8`, the halfword,
  dual and most-significant-word multiplies, `UMAAL`) and FPv4-SP-D16
  (arithmetic, `VFMA` family, `VSQRT`, `VCMP`, `VCVT` forms, `VMOV` forms,
  `VLDR`/`VSTR`, `VLDM`/`VSTM`/`VPUSH`/`VPOP`, `VMRS`/`VMSR`), FPSCR, CPACR
  with the NOCP fault, CONTROL.FPCA, FPCCR/FPCAR/FPDSCR and lazy stacking.
- APSR.GE through `MSR`/`MRS` and in stacked xPSR; CPUID and MVFR0/1 of
  the Cortex-M4; CCR.STKALIGN set at reset on the Cortex-M4.
- Timing: the Cortex-M4's one-cycle multiplier, one cycle for DSP
  instructions, the FPU's counts (1; 3 for multiply-accumulate; 14 for
  divide and square root; 2 for `VLDR`/`VSTR`; 1 + N for the multiples).
  The flash and ART accelerator model of the F4 is not made yet (SN-296).

## Gate G12a

Measured on the owner's laptop, Windows, MSVC Release, 2026-11-11
(`m4_tests`, ctest `emu-m4-unit`, `emu-m4-differential`,
`emu-m4-benchmark`).

| ID | Result |
|---|---|
| G12a.1 | **Met by test, not yet against the document's text.** Every DSP form: 20 000 random and edge operands each (820 000 cases), result, Q and GE equal a reference written apart from the core. FPU: the special cases (NaN choice, default NaN, invalid operations, signed zeros, overflow by rounding mode, underflow before rounding, flush-to-zero, fused multiply-add, comparisons, saturating conversions, half precision in both formats) in directed tests; the thirteen three-register operations with 5 000 random operands each in the four rounding modes with DN and FZ |
| G12a.2 | **Met.** 6 000 000 results (1 000 000 per operation: add, subtract, multiply, divide, square root, fused multiply-add) and 3 457 718 conversions against the host in the four rounding modes: 0 differences in value; 0 in flags for the five operations whose host flags are reliable (the C library's `fma` flags are not compared). The one expected difference is excluded: ARM detects underflow before rounding, x86 after it, so a result rounded up to the smallest normal number is flagged by ARM alone. Shared instructions: 393 376 random encodings from the same state on both cores, 0 different registers, flags, memory or fault status, with CCR.STKALIGN set on both (its reset value is the one real difference) |
| G12a.3 | **Met.** Seven scenarios: lazy stacking with the handler using the FPU and not using it, a thread that never used the FPU, lazy stacking off, automatic preservation off, an unaligned stack pointer, and a nested interrupt in a handler that used the FPU; frames of 0x68 and 0x20 bytes, EXC_RETURN 0xFFFFFFE9/E1/F9, FPCCR.LSPACT, FPCAR, the new context's FPSCR from FPDSCR. The FPU off (CPACR), privileged-only access and double-precision encodings give NOCP, NOCP and UNDEFINSTR. A context switch through PendSV is not tested yet (it needs an RTOS firmware: with the machine, SN-294) |
| G12a.4 | **Met.** Integer loop: Cortex-M3 135 MIPS, Cortex-M4F 133 MIPS (98 %; runs vary by a few percent). A third of FPU instructions: 70 to 78 MIPS (gate: 60). The FPU's register arithmetic is on the run loop's fast path |
| G12a.5 | **Met.** Every 16-bit pattern and 10 000 000 random 32-bit ones decode; 200 random flash images, a third of their halfwords DSP or FPU heads, run 100 000 cycles each with the FPU on without a crash or hang |
| G12a.6 | **Met.** The STM32F103's 53 tests (`emu-unit`, with the Blue Pill timing comparison) pass unchanged. Speed against `main` built beside it, `simnodus-emu`, 3 s of firmware, best of five: `hal_clock` 163.7 → 162.3 MIPS (-0.9 %), `cycle_loops` 120.0 → 119.1 MIPS (-0.8 %). A first layout of the new registers cost 2.4 to 3.1 %; they were moved after the core's other members |

**Limits of this gate as it stands.**
- The reference of G12a.1 and the core were written by the same hand from
  the same manual: an error of understanding would be in both. The
  host comparison (G12a.2) does not have this weakness for the FPU's
  arithmetic; the DSP instructions and the FPU's instruction plumbing do.
  The owner's chip is the check that removes it (G12b.5, G12b.9).
- Cycle counts are the TRM's as known, not yet read against it or
  measured (SN-296).

## The STM32F407 machine (SN-294)

`src/emu/stm32f407.*`, run with `simnodus-emu --stm32f407 firmware.elf
[--crystal Hz]`, tests `f4_tests` (ctest `emu-stm32f407`).

**Built.**
- Memory map: 512 KiB of flash with its alias at 0, 128 KiB of SRAM, 64 KiB
  of CCM memory, the bit-band aliases, system memory with the unique ID and
  the flash size, DBGMCU (device 0x413), DWT's cycle counter.
- RCC: HSI 16 MHz, the HSE with the frequency of `--crystal` (later: of the
  Crystal on the diagram), the main PLL (`PLLCFGR`), the AHB and APB
  prescalers, the reset flags. With no crystal the HSE never becomes ready
  and the log says why. A flash latency too low for HCLK gives a warning.
- PWR (the regulator ready at once) and the flash interface's registers
  (programming from the firmware is not simulated and says so).
- GPIO ports A to I in the F4's form (mode, output type, pull-up and
  pull-down, BSRR, alternate function numbers), SYSCFG's EXTI port
  selection, EXTI with its 23 lines.
- USART1 to USART3, UART4, UART5 and USART6 with frames on the pins. The
  USART is the STM32F103's, moved to `stm32_usart.hpp` behind a host
  interface, with the F4's oversampling by 8. A port's TX and RX are on the
  first of its candidate pins that is in alternate function mode with the
  port's function number (7 for USART1 to USART3, 8 for UART4, UART5 and
  USART6). The table was written from the datasheet as known and is to be
  checked against its text.
- Every other peripheral of the chip is a stub that keeps its registers and
  says once that it is not simulated.
- The core: 82 interrupts (`Cpu::set_irq_count`) and a decoded-code cache of
  the flash's size (`Cpu::set_code_cache_bytes`). Pending exceptions are a
  bit set now: "nothing pending" is one comparison in the run loop.

**Owned firmware** (`examples/firmware/f4_*`, no HAL: the STM32CubeF4
package is not installed on this computer; `tools/build_f4_firmware.py`):
`f4_blink`, `f4_clock`, `f4_float`, `f4_button`. Their README says how to
run them on the owner's board.

**Measured on the host** (owner's laptop, Release):

| Criterion of G12b | Result so far |
|---|---|
| G12b.1 | `f4_blink` on the HSI: PC13 toggles every 500 ms exactly; the busy loop of 1 000 000 turns takes 501.254 ms (8.02 cycles a turn with the TRM's counts and no flash wait state). Not compared with the chip |
| G12b.2 | `f4_clock`: no crystal: the firmware waits and the log explains; 25 MHz: `SYSCLK 168000000 HCLK 168000000 PCLK1 42000000 PCLK2 84000000`, one bit on PA9 is 8.679 us (115226 baud); 8 MHz with the same firmware: HCLK 53.76 MHz and 36872 baud on the pin |
| G12b.3 | GPIO output types, pulls, BSRR, analog mode; a serial port reaches PA9 only with function 7, and moves to PB6; EXTI line 3 from port D through SYSCFG. Speed bits are kept and not simulated; 5 V tolerance is the part's business (SN-295) |
| G12b.4 | Bytes of USART1 and USART2 reach the terminal and the pins, also with OVER8; a terminal's byte sets RXNE. The six ports share one model; only two are exercised by tests so far |
| G12b.5 | `f4_float` prints `A 1A3EE223`, `B 183ECD01`, `C 596EC23A`, `I 46B57D98`, `x 40182DAE`, `done`, the same with the Cortex-M4's cycle counts and with one cycle per instruction. **To be compared with the chip** |
| G12b.6 | Power-on, software and pin resets set their flags in `RCC_CSR`; RMVF clears them. NRST and BOOT0 from the circuit: SN-295 |
| G12b.8 | `f4_blink` at 16 MHz: 7.7 times real time; `f4_clock` at 168 MHz, which waits in a busy loop: 0.85 times real time, 86 MIPS. As planned: firmware that never sleeps at 168 MHz runs below real time |
| G12b.10 | Two runs of `f4_float` give the same pin changes and bytes at the same times |
| G12b.11 | The STM32F103's 53 tests pass with the USART moved; its speed with room for 96 interrupts, best of five: `hal_clock` 163.5 -> 163.6 MIPS, `cycle_loops` 119.2 -> 117.3 (-1.6 %), `freertos_tasks` 151.3 -> 148.7 (-1.7 %). A plain enlargement of the arrays cost 3 to 7 %; the bit set and moving the arrays after the hot members brought it back |

Not done in SN-294: GDB for the F407, the Register Viewer's map, the part
(G12b.7 and the rest of the integration: SN-295), and the board comparison
(G12b.9: SN-296).

## The part on the diagram (SN-295)

**Built.**
- The part `stm32f407vet6_lqfp100` (`tools/generate_stm32_parts.py`): the
  100 pins of the package in two columns in the package's order, with their
  numbers. The supply pins of a rail are joined inside the chip; NRST has
  its pull-up; the 80 pins of ports A to E drive the circuit through the pin
  model. The pinout was written from the datasheet as known and is to be
  checked against its text.
- The adapter (`Stm32F407Machine` in `src/mcu/mcu_machine.cpp`): pins
  "PA0" to "PE15", "NRST" and "BOOT0"; serial ports "usart1" to "usart6"
  ("uart4", "uart5"); the registers and the FPU's for the Register Viewer.
- The crystals come from the diagram (`stm32f407_options` in the session):
  a Crystal part between PH0 and PH1 is the HSE, one between PC14 and PC15
  the 32.768 kHz crystal. The Crystal part takes up to 50 MHz now (it stopped
  at 20 MHz).
- NRST from the circuit holds the chip in reset and starts it again when
  released; BOOT0 is read when a reset ends, and high keeps the chip from
  starting with a message (the system bootloader is not simulated).
- GDB: one Cortex-M target for the F103 and the F407; the F407's
  description adds S0 to S31 and FPSCR in a feature of its own
  (`org.simnodus.fpu`), since gdb's `org.gnu.gdb.arm.vfp` wants 64-bit D
  registers.
- The Register Viewer's map of the chip (`register_map("stm32f407")`): the
  core and the FPU, RCC, FLASH, PWR, GPIOA to GPIOE and GPIOH, SYSCFG,
  EXTI, the six serial ports, SysTick, NVIC, SCB. A test reads every
  register of the map from the machine.
- Example 67 (`67_STM32F407_clock_and_serial.snx`): the chip, a 25 MHz
  crystal with its capacitors, an LED on PC13, a reset button, USART1 at
  115200 baud. `f4_clock` sleeps between ticks now (WFI).
- The guide's section *STM32F407VET6 (experimental)*, in English and pt-BR.

**Measured on the host.**

| Criterion of G12b | Result |
|---|---|
| G12b.2 | In the circuit: `crystal_hz` is 25000000 from Y1; the terminal shows `SYSCLK 168000000 HCLK 168000000 PCLK1 42000000 PCLK2 84000000` and the ticks; without Y1 the request carries no frequency |
| G12b.6 | NRST low holds the chip (no instruction runs, time passes) and its release restarts the firmware with PINRSTF; BOOT0 high at a reset: the message, and nothing runs |
| G12b.7 | The GDB target: registers 0 to 20, S0 to S31 at 21 to 52, FPSCR at 53, memory read and written, the flash written as `load` does. **Not yet tried with STM32CubeIDE itself** (the owner's installation). The Register Viewer shows RCC, GPIO and the core's registers of the running example (GUI test) |
| G12b.8 | Example 67 in the circuit: 8.4 times real time with the firmware sleeping between ticks; 0.79 times while it waited in a busy loop. `simnodus-emu`: 145 times and 0.85 times |

**Limits of SN-295.**
- The supply voltage is not read: the chip runs with any supply (the AVR
  chips read theirs).
- PH0 and PH1 are the crystal's pins only; as GPIO they are not connected
  to the circuit.
- No ADC yet, so no analog pin; 5 V tolerance is not modelled.
- The timing of instructions still has no flash model (SN-296).

## Timers, ADC and DAC (SN-297)

What was built:
- One timer model for the F103 and the F407 (`stm32f1_timer.*`, configured
  per timer: advanced or not, channels, 16 or 32 bits, APB1 or APB2, its
  interrupt lines). New in it, for both chips: the dead time of the
  complementary outputs (DTG with CKD), a complementary output alone,
  the encoder interface (SMS 1 to 3), the trigger output (MMS 2 to 7),
  capture on both edges, and registers masked to the channels a timer has.
  The F103's timer tests pass unchanged.
- The F407's TIM1 to TIM14 with the datasheet's pins and function numbers,
  the timer clock at twice PCLK when the bus is divided, and the shared
  interrupt lines (TIM1's break with TIM9, TIM6 with the DAC...).
- `stm32f4_analog.*`: ADC1 to ADC3 with the common registers (see the
  header for what is simulated) and the DAC with its triangle wave and
  timer triggers.
- GPIO: a pin in alternate function mode is an input while no peripheral
  drives it (RM0090 figure 26). Before, it always drove, so a capture
  channel or a USART's RX on such a pin never saw the circuit.
- The circuit: the DAC drives PA4 and PA5 as an analog source behind a
  resistance (`PinState::analog_volts`, `analog_resistance`): 15 kΩ without
  the buffer (DS8626's R_O), 10 Ω with it, clamped to 0.2 V to VDD - 0.2 V.
  The 10 Ω is an approximation: the datasheet gives the buffer's minimum
  load (5 kΩ) and its range, not an output impedance. The engine protocol
  is unchanged (the drive is computed in the engine).
- Firmware `f4_analog` (PWM, ADC on a timer trigger, both DAC channels) and
  example 68; the Register Viewer's map has TIM1 to TIM14, the ADCs and the
  DAC. `f4_pwm` of the plan is not a separate firmware: `f4_analog` and the
  machine tests cover the PWM.

| Criterion | Result on the host |
|---|---|
| G12c.1 | TIM1 PWM 10 us with CCR1 40: high 1.5 us and low 8.5 us on PA8, the complement 6.5 us and 3.5 us on PA7, each edge 1 us (16 clocks of dead time) after the other's, for 1000 periods; TIM2 with ARR 0x1FFFF (32 bits) gives 4.096 ms pulses; capture on both edges within one count; encoder mode 3 counts 4 per cycle up and down, mode 1 counts 2, underflow reloads ARR with an update. The F103's tests unchanged. Not done: center-aligned modes, the break input, slave modes 4 to 7, the input filters (reported once) |
| G12c.2 | Codes ⌊V·2ⁿ/3.3⌋ at 12 and 8 bits, left alignment, conversion times of 15 and 11 ADC cycles at 8 MHz, scan with EOCS, overrun, injected channel 16 with its offset, VREFINT, ADC3's channels, a TIM3 TRGO start 100 us apart, the end-of-conversion interrupt; DMA (SN-298, see G12c.5). Not compared with the chip |
| G12c.3 | The DAC's codes on its pins in the three alignments and the dual registers, a TIM6 trigger at its exact time, the triangle 101-102-103-102-101-100, the software trigger; in example 68 the code 2867 gives 2.310 V on PA4 in the circuit. Through DMA (SN-298): a table of four codes on TIM6's 10 us trigger gives 100, 1000, 2000, 3000, 100, 1000 at the trigger times, and a stopped stream an underrun with its interrupt; a sine is the same mechanism with a longer table |

Example 68 runs at about 1.6 times real time (16 MHz, the firmware sleeping
between conversions, the PWM at 20 kHz and the triangle's 10 000 steps a
second).

**Limits of SN-297.** The VREF pin is not read (VREF+ is 3.3 V); the
temperature sensor gives a fixed 0.76 V; a DAC output is not limited by its
load current; the ADC's sampling capacitor is not modelled (a high source
impedance reads as a low one).

## SPI, I2C, DMA, RTC and the system peripherals (SN-298)

What was built:
- SPI and I2C: the STM32F103's models (SN-204, SN-205) take their number,
  bus, interrupts, DMA requests and pins from the chip (`SpiConfig`,
  `I2cConfig`, `SpiHost::spi_pins`, `I2cHost::i2c_pin`); the F103 keeps
  its tables and its tests pass unchanged. On the F407: SPI1 to SPI3 on
  functions 5 and 6, I2C1 to I2C3 on function 4, with the datasheet's
  pins; the F4's extra registers (CR2.FRF, I2SCFGR, I2SPR, FLTR) are kept
  and what they would do is reported once.
- `stm32f4_dma.*`: DMA1 and DMA2, eight streams each, the request tables
  of RM0090 (tables 42 and 43) for the peripherals SimNodus simulates;
  normal, circular and double-buffer modes, memory to memory on DMA2,
  priorities, the flags and interrupts, a software disable setting TCIF as
  on the chip. In FIFO mode the memory side is moved in peripheral-sized
  items through consecutive addresses, which leaves the same bytes as the
  FIFO's packing; thresholds, bursts and the FIFO error flags are not
  modelled.
- The ADC asks for a transfer after each regular result with CR2.DMA (a
  result not taken is an overrun); the DAC asks at each trigger and flags
  DMAUDR when a request is still waiting at the next one.
- `stm32f4_system.*`: the RTC (BCD calendar with weekday, leap years,
  24-hour or AM/PM, initialization mode, the key, ADD1H and SUB1H, alarms A
  and B with masks on EXTI 17, the wakeup timer on EXTI 22 counting either
  RTCCLK / 16 to / 2 or the calendar's 1 Hz edges, twenty backup
  registers) and the RNG. RTCCLK comes from RCC_BDCR: the LSE when a
  crystal is between PC14 and PC15, the LSI at 32 kHz, or HSE / RTCPRE.
  BDCR and the RTC need PWR_CR.DBP, as on the chip; a power-on or BDRST
  clears them. The IWDG (on the 32 kHz LSI), WWDG and CRC are the F103's.
  The 4 KiB backup SRAM keeps its contents through resets.
- Firmware `f4_buses`, `f4_rtc`, `f4_station` with the shared
  `f4_peripherals.h`; `f4_hal_buses` with the STM32CubeF4 HAL, built by
  `tools/build_f4_firmware.py` from a `cube.json` like the Blue Pill's HAL
  examples (`stm32f407ve_cube.ld`, newlib-nano); examples 69, 70 and 71;
  the Register Viewer's map.
  The other F4 ELFs were rebuilt because `f4.h` gained optional vectors
  (their code is the same; their debug line numbers moved).

| Criterion | Result on the host |
|---|---|
| G12c.4 | SPI1 at PCLK / 8: 16 edges 250 ns apart, MOSI with 0xA5 at the rising edges, MISO read as 0xFF; SPI3 only on function 6. In example 69, SPI1 counts on the 74HC595 (Q0 changes every 250 ms in the circuit), SPI2 with 16-bit frames drives the MAX7219, I2C1 writes "SimNodus" into the 24C02, polls its write cycle and reads it back, and reads 23.5 °C from the LM75. I2C1 alone: SB, an address nobody acknowledges gives AF, SCL 5 us high and 5 us low at 100 kHz. With the HAL: `f4_hal_buses`, an STM32CubeIDE-style project on STM32CubeF4 1.28.3 (HAL_SPI_Transmit, HAL_I2C_Mem_Write, HAL_I2C_IsDeviceReady, HAL_I2C_Mem_Read, HAL_UART_Transmit_DMA), gives the same lines and the same count in example 69's circuit |
| G12c.5 | The timers' requests: TIM3's update event loads CCR1 from a table of three duty cycles (20, 50, 80 of 100 counts, a new one every period), TIM2's captures go to memory 160 counts (10 us) apart, TIM6's update event writes a wave of four codes into the DAC every 10 us. Memory to memory on DMA2 (four words, half and full transfer, the stream's interrupt), refused on DMA1 with a message; USART2 sends "hello" through DMA1 stream 6 and receives into a circular buffer on stream 5 (the fourth byte wraps around, NDTR reloads); ADC1 scans two channels continuously into a circular buffer of four on DMA2 stream 0 without overrun; the DAC's table above |
| G12c.6 | The calendar on the LSE from 2024-02-28 23:59:58 (a Wednesday) gives 2024-02-29, a Thursday, 00:00:01 after 3 s; alarm A on the seconds fires between 6.999 s and 7.001 s with its interrupt; the wakeup timer on the 1 Hz clock every 2 s; the backup registers, the calendar and the backup SRAM survive a software reset and a power-on clears them; on the LSI with 128 × 250 the seconds come at 1 s ± 1 ms. In example 70 the watchdog resets the chip while SW2 is held and the calendar and the start count go on. Without DBP writes are ignored and reported; without a clock INITF never sets |
| G12c.7 | On the owner's board (2026-11-13), examples 67, 68, 69, 70 and 71 behaved as in the simulator (the owner's comparison by eye and on the serial port); in example 71 the time on the serial port and the PWM_AVG and PA4 voltages were similar to the simulator's (the owner's reading, values not written down). Example 69 first stayed dark on the MAX7219: a wrong breadboard connection; fixed, it works with the STM32's 3.3 V levels on the MAX7219 on 5 V. A 24C08 in place of the 24C02 gave `eeprom SimNodus`. Since then `f4_buses` and `f4_hal_buses` send the MAX7219's setup again every second, and the run warns when a chip on 5 V gets 3.3 V highs below its datasheet's 0.7 of the supply (the simulator reads them as high, as this MAX7219 did). In the simulator, example 71 at 168 MHz prints "12:00:01 2026-11-12 lm75 23.5 pwm 235 dac 940" every second, PWM_AVG is 0.776 V (23.5 % of 3.3 V) and PA4 0.757 V (940 / 4095 of 3.3 V); at 30 °C the line says "pwm 300 dac 1200" |

Also: the IWDG at LSI / 4 with 320 counts resets after 40 ms (IWDGRSTF),
the WWDG from 0x7F after 16.384 ms (WWDGRSTF), CRC of 0x12345678 is
0xDF8A8A2B, and the RNG gives its first number 834 ns after it starts, the
same on every run.

Example 69 runs at about 0.4 times real time, as the F103's example 26
(0.67): the MAX7219 scans its matrix 800 times a second. Example 70 runs at
about 5 times and example 71 at about 6 times real time (the core sleeps).

**Limits of SN-298.** The HAL firmware covers SPI, I2C and UART with
DMA, not the RTC or the ADC through the HAL; the timers' trigger and
commutation DMA requests are not wired; DMA transfers take no bus time; SPI and
I2C are masters only, without I2S, CRC, TI mode or the I2C noise filters;
the LSI is exactly 32 kHz (a chip's varies from 17 to 47 kHz); the RNG's
numbers are a fixed sequence.

## Gate review (SN-299)

Prepared on 2026-11-13 for the review with the owner. "Host" means measured
by the tests on the owner's laptop; "board" means on the owner's STM32F407VET6
board (25 MHz crystal) with the owner's instruments.

**`f4_timing`** (new, `examples/firmware/f4_timing`): the five loops of the
plan grew to 27, as `cycle_loops` did for the Blue Pill: integer, loads and
stores (from SRAM and from the flash), multiply and divide, branches and
calls, `LDM` and `PUSH`/`POP`, `SMLAD`, the FPU's add, multiply, fused
multiply-add, divide, square root, `VLDR` and `VMOV`, a 2 KiB loop (twice
the ART accelerator's instruction cache), three C functions (sort, CRC-32,
a float filter) and SysTick entries with and without lazy stacking. They run
at 16 MHz with no wait state, then at 168 MHz with 5 wait states in three
setups: prefetch and both caches (STM32CubeMX's), the caches without
prefetch, and the accelerator off. In the simulator the 16 MHz block has
the Cortex-M4 manual's counts (`alu 11002`, `vadd 11003`, `vfma 27002`,
`vdiv 115003`: one, one, three and fourteen cycles); the 168 MHz blocks are
the provisional model (no wait state with any of the accelerator on, every
flash access paying the latency with it off), which the board's report will
replace. Test: `f4: the timing firmware reports its loops in four flash
setups`.

| Criterion | Host | Board | Verdict |
|---|---|---|---|
| G12a.1 instructions against the pseudocode | met by test | `f4_float` (G12b.5) is the check | **met on the host**; the documents' text not read |
| G12a.2 random differential | met (6 000 000 results, 0 differences) | — | **met** |
| G12a.3 lazy stacking | met (seven scenarios) | — | **met**; a PendSV context switch with an RTOS not tested |
| G12a.4 speed of the core | 133 vs 135 MIPS; 70-78 MIPS with FPU | — | **met** |
| G12a.5 decoding robustness | met | — | **met** |
| G12a.6 the Cortex-M3 unchanged | met (-0.8 to -1.7 %) | — | **met** |
| G12b.1 `f4_blink` period in cycles | 501.254 ms for the busy loop | PB0's period not measured yet | **open: board** |
| G12b.2 clock tree from the crystal | met | example 67 printed the `SYSCLK` line and the ticks at 115200 baud | **met** |
| G12b.3 GPIO and alternate functions | met | examples 67-71 drove their pins as simulated | **met**; 5 V tolerance not modelled |
| G12b.4 the six serial ports | met for USART1 and USART2 | USART1 | **met for two ports**; the other four share the model |
| G12b.5 `f4_float`'s checksum | the five lines | not run yet | **open: board** |
| G12b.6 resets | met | example 70 behaved as simulated (the watchdog's reset not reported separately) | **met on the host** |
| G12b.7 debugging with STM32CubeIDE | the GDB target and the Register Viewer by test | not tried yet | **open: board** |
| G12b.8 speed in the circuit | `f4_blink` 7.7x; examples 70, 71 5-6x; busy loop at 168 MHz 0.79-0.85x, stated in the guide | — | **met** |
| G12b.9 board comparison and calibration | `f4_timing` ready | `f4_clock` matched (example 67); `f4_float`, the blink period and `f4_timing` not run | **open: board** |
| G12b.10 repeatable runs | met | — | **met** |
| G12b.11 the other chips unchanged | ctest, `examples_pass.py`, `examples_gui_pass.py` | — | **met** |
| G12c.1 timers | met (limits in SN-297) | example 71's PWM | **met on the host** |
| G12c.2 ADC | met | example 68 as simulated | **met on the host**, labeled approximate |
| G12c.3 DAC | met | example 71's PA4 similar | **met on the host** |
| G12c.4 SPI and I2C with the parts and the HAL | met | example 69 (after a wiring fix), a 24C08 for the 24C02 | **met** |
| G12c.5 DMA streams | met | examples 69, 71 print through DMA | **met on the host** |
| G12c.6 RTC | met | example 70 as simulated | **met on the host** |
| G12c.7 `f4_station` on the board | the trace | similar time, PWM_AVG and PA4 (by eye, values not written down) | **met by the owner's comparison**, not to the plan's tolerances |

**Reading of DE12.** By the plan's outcomes, G12a passes and G12b passes
except the three board items; the part stays **experimental** until
G12b.9. Nothing measured asks for a stop or a speed week. What closes
E-12 is one session on the bench, about half an hour:

1. `f4_float`: the five checksum lines and `done` (G12b.5).
2. `f4_timing`: one whole report from `f4_timing v1` to `end` (G12b.9);
   the timing model is then calibrated from it, as SN-181 did for the
   Blue Pill, with a test holding the numbers within 2 %.
3. `f4_blink` with the logic analyzer: PB0's half period (G12b.1, G12b.9).
4. STM32CubeIDE's debugger against example 67 (G12b.7): a breakpoint,
   stepping, the FPU registers, a peripheral register.

Then the ADR for the STM32F4 on the own emulator, and the owner's
decision on dropping the experimental label.

## Next

The owner's bench session above (SN-296); then the calibration, the ADR and
DE12 (SN-299).
