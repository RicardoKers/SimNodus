# E-12: STM32F407VET6 on the own emulator — plan

Date: 2026-11-11. Tasks: SN-292..SN-299 in the [backlog](../planning/BACKLOG.md).
Status: **approved by the owner on 2026-11-11**; work starts with SN-292 and SN-293.
The criteria below are recorded before any code, as for
[E-07](E-07-results.md), [E-08](E-08-plan.md) and [E-10](E-10-plan.md), and
are not relaxed after a run: a failed criterion leads to a recorded decision.

## Question

Can SimNodus's own emulator run **unmodified** STM32F407VET6 firmware (the
ELF files STM32CubeIDE produces, with the FPU on as its projects have it by
default) in the circuit, as a bare chip whose clock comes from the crystal
on the diagram, with results equal to the owner's chip, at a speed that is
still useful in class, and without changing anything the STM32F103, ESP32-C3
and AVR paths do today?

## Owner's decisions (2026-11-11)

- The part is the **bare chip** (LQFP-100, every pin on the symbol), not a
  board: projects differ, and so does the crystal. The oscillator's
  frequency comes from a Crystal part on the diagram, as for the AVR chips.
- The owner has a test board with the STM32F407VET6, a 3.3 V regulator, a
  25 MHz crystal, SWD and every pin on a breadboard. It is the final
  reference of the comparisons; it does not define the part.

## What exists and what is new

| Area | Today (STM32F103) | STM32F407 |
|---|---|---|
| Core | Cortex-M3, cycle counts calibrated on a Blue Pill | **Cortex-M4F**: the DSP instructions and the single-precision FPU (FPv4-SP-D16) with lazy stacking; new |
| Memory | 64 KB flash, 20 KB SRAM | 512 KB flash, 112 + 16 KB SRAM, 64 KB CCM at 0x10000000 |
| Clock | HSI 8 MHz, HSE 8 MHz fixed, PLL of the F1 | HSI 16 MHz, HSE from the Crystal part (4 to 26 MHz), the main PLL with `PLLCFGR`, up to 168 MHz; new RCC |
| GPIO | `CRL`/`CRH`, AFIO remaps | `MODER`/`OTYPER`/`OSPEEDR`/`PUPDR`/`AFR`, ports A to E and PH0/PH1, SYSCFG; new |
| USART, SPI, I2C, EXTI, IWDG, WWDG, CRC | own classes behind host interfaces | the same register design: the classes are reused with more instances (`OVER8` added to the USART) |
| Timers | TIM1 to TIM4, 16 bits | the same design; TIM2 and TIM5 with 32 bits, TIM6 to TIM14 added |
| ADC | ADC1/2 of the F1 | ADC1 to ADC3: resolution, the common registers, other sample times; adapted |
| DMA | channels | streams with a channel selection and a FIFO; new |
| RTC | counter | calendar (BCD), alarms, backup registers; new |
| DAC | none | two channels on PA4 and PA5: the first analog output of a chip into the circuit; new |
| Debugging, Register Viewer, Memory Viewer, terminal | work per chip | a target description with the FPU registers, a register map |

## Scope

**In (G12a–G12c):**
- the Cortex-M4F core beside the Cortex-M3 (one decoder, the chip's features
  chosen when the machine is made);
- the part `stm32f407vet6_lqfp100`: 82 I/O pins, NRST, BOOT0, VDD/VSS,
  VDDA/VSSA, VREF+, VBAT, VCAP1/2, with the 3.3 V pin model from the
  datasheet's tables (5 V tolerant inputs marked as such);
- the clock tree with the Crystal part between PH0 (OSC_IN) and PH1
  (OSC_OUT), and a 32.768 kHz Crystal between PC14 and PC15 for the RTC;
- the peripherals listed under the gates;
- ELF loading, `simnodus-emu --chip stm32f407`, integration through
  `McuMachine`, GDB with STM32CubeIDE, the register map of the Register
  Viewer, examples and guides (English and pt-BR);
- the part is marked **experimental** until the board comparison passes.

**Out (later, each a task of its own if asked for):** USB OTG, Ethernet,
SDIO, CAN, FSMC, DCMI, I2S, the system bootloader (BOOT0 high: a message
says it is not simulated), the MPU, option bytes and read protection, Stop
and Standby beyond what WFI needs, flash programming by the firmware, the
oscillator simulated in the circuit, HSE bypass with a clock from the
circuit, other F4 chips and packages.

## Prerequisites (SN-292, before emulator code)

1. **Pinned references**, each by document number and revision in the
   report: RM0090 (reference manual), the STM32F405/407 datasheet
   (DS8626), PM0214 (Cortex-M4 programming manual), the ARMv7-M
   Architecture Reference Manual (DDI 0403) for the instruction
   pseudocode, the Cortex-M4 Technical Reference Manual for cycle counts,
   and the chip's errata sheet (ES0182). The PDFs are not stored in the
   repository.
2. **Toolchain.** The arm-none-eabi-gcc of the STM32CubeIDE already
   installed (version recorded) and the STM32CubeF4 package for the HAL
   examples. Every image used is identified by SHA-256.
3. **Oracles.**
   - The **host's own IEEE 754 single precision** (`<cfenv>` rounding
     modes) for the arithmetic of the FPU instructions.
   - The **Cortex-M3 path** for everything the two cores share: the same
     random blocks run on both and must agree.
   - **QEMU** (a machine with a Cortex-M4, `netduinoplus2` or
     `olimex-stm32-h405`) as a separate, test-only process if a Windows
     build is at hand; it is GPL: never linked, never copied, its source
     not read while the corresponding model is written. Optional.
   - The owner's **STM32F407VET6** through SWD is the final reference: a
     self-checking firmware runs the same instruction blocks on the chip
     and reports a checksum.
4. **Owner's bench**, to confirm: the ST-Link (or other SWD probe) used
   with STM32CubeIDE, a USB-serial adapter, the oscilloscope and logic
   analyzer of E-10, and whether the board has a 32.768 kHz crystal.

## Owned firmware set

Sources in `examples/firmware/f4_*`, built with the pinned compiler, each a
project with a README plus the ELF it was built to. The clock is chosen in
the source: the HSI at 16 MHz, or the PLL at 168 MHz from a 25 MHz crystal
(`HSE_VALUE`), as on the owner's board.

| Firmware | Covers | Compared on the board |
|---|---|---|
| `f4_blink` | reset state, HSI 16 MHz, a GPIO output, a busy delay and SysTick | period |
| `f4_clock` | HSE 25 MHz, PLL to 168 MHz, flash latency, the USART at 115200 printing `SystemCoreClock` | bytes; baud rate on the analyzer |
| `f4_float` | the FPU as CubeIDE uses it: `printf` of floats, a filter and a sine table in single precision, the DSP instructions, an interrupt that uses the FPU while the main loop does (lazy stacking); prints a checksum of all results | checksum, byte for byte |
| `f4_timing` | five loops of known instruction mixes (integer, branches, loads and stores, multiply and divide, FPU) timed with a hardware timer | counts per loop |
| `f4_button` | an input with pull-up, EXTI on both edges | response seen on the analyzer |
| `f4_pwm` | TIM1 complementary PWM, TIM2 with 32 bits, input capture on TIM3 | frequency and duty |
| `f4_analog` | DAC channel 1 as a sine, wired to an ADC input; injected and regular conversions, the temperature sensor | DAC on the oscilloscope; readings against a known voltage |
| `f4_buses` | SPI1 to a 74HC595, I2C1 to an LM75 and a 24C02, USART2 with DMA | bytes and bus frames on the analyzer |
| `f4_rtc` | the calendar on the 32.768 kHz crystal (or the LSI), an alarm, the backup registers | one-second tick |
| `f4_station` | all together with a 1 ms tick: read a potentiometer, set a PWM duty and the DAC from it, filter in floating point, report over serial | the whole trace |

## Work packages

| # | Package | Done when |
|---|---|---|
| 1 | SN-292: prerequisites above; the feature switch in `src/emu/cpu.*` (Cortex-M3 or Cortex-M4F) with nothing changed for the M3 | Recorded; the F103 suites and benchmark unchanged |
| 2 | SN-293: the Cortex-M4F core. DSP: saturating and parallel add and subtract, the halfword and dual multiplies, `UMAAL`, `SSAT16`/`USAT16`, `PKH`, the extends with add, `SEL`, the GE flags. FPU: `VADD` to `VSQRT`, `VFMA` family, `VCMP`, the `VCVT` forms (integer, fixed point, rounding), `VMOV` forms, `VLDR`/`VSTR`, `VPUSH`/`VPOP`, `VLDM`/`VSTM`, `VMRS`/`VMSR`; FPSCR (flags, rounding mode, cumulative exceptions, default NaN and flush-to-zero); CPACR with the NOCP UsageFault; lazy stacking (FPCCR, FPCAR, CONTROL.FPCA, EXC_RETURN bit 4); the cycle counts of the TRM | Instruction tests pass → **G12a** |
| 3 | SN-294: `src/emu/stm32f407.*`: memory map, RCC with the Crystal's frequency, PWR, FLASH interface (latency), GPIO with the alternate function table of the datasheet, SYSCFG, EXTI, the NVIC's 82 interrupts, USART1 to USART3, UART4, UART5 and USART6, DBGMCU and the ID registers | `f4_blink`, `f4_clock`, `f4_float`, `f4_button` run in the CLI |
| 4 | SN-295: integration: the `McuMachine` adapter, the part with its pin model and power pins, the crystal read from the diagram (no crystal with HSE on: HSERDY never sets and the log says why), NRST and BOOT0 from the circuit, GDB with the FPU registers, the Register Viewer's map, examples and guides | GUI test and examples pass |
| 5 | SN-296: board comparison of the four first firmware, and the timing calibration from `f4_timing` (flash wait states and the ART accelerator as measured) | → **G12b**, decision **DE12** |
| 6 | SN-297: timers (TIM1 to TIM14), ADC1 to ADC3, the DAC as an analog output into the circuit | `f4_pwm`, `f4_analog` |
| 7 | SN-298: SPI1 to SPI3, I2C1 to I2C3, DMA1 and DMA2 streams, the RTC calendar with backup registers, IWDG, WWDG, CRC, RNG (a fixed sequence, labeled) | `f4_buses`, `f4_rtc`, `f4_station` → **G12c** |
| 8 | SN-299: report `docs/experiments/E-12-results.md`, the board comparison of the remaining firmware | Gates reviewed with the owner |

## Gate G12a — core

| ID | Criterion |
|---|---|
| G12a.1 | Every DSP and FPU instruction the Cortex-M4F has: result, APSR (with GE and Q) and FPSCR flags equal the ARMv7-M pseudocode, in all four rounding modes, with NaNs, infinities, denormals and the default-NaN and flush-to-zero modes |
| G12a.2 | Random differential. FPU arithmetic against the host's IEEE single precision: at least 1 000 000 operand pairs per operation, including the special values, with zero differences in result and flags. Instructions shared with the Cortex-M3: random blocks give the same registers and flags on both cores |
| G12a.3 | Lazy stacking: an interrupt that uses the FPU, one that does not, nested ones and a context switch through PendSV keep S0 to S31 and FPSCR of every context, with the stack frames (26 or 8 words) and EXC_RETURN values of the architecture manual. An FPU instruction with CPACR off gives the NOCP UsageFault |
| G12a.4 | Speed, Release build on the owner's laptop on Windows (MSVC), Linux recorded on CI: the Cortex-M4F core within 5 % of the Cortex-M3's MIPS on the integer benchmark, and at least 60 MIPS on a mix with a third of FPU instructions |
| G12a.5 | Every 16-bit and a random sample of 10 000 000 32-bit encodings decode without crashing the host; an undefined one gives the UsageFault of the chip. 200 random flash images run 100 000 cycles each without a crash or hang |
| G12a.6 | The Cortex-M3 is unchanged: the F103's instruction tests, timing tests and examples pass as they are, and its MIPS in `emu-benchmark` stay within 3 % |

## Gate G12b — firmware in the circuit

| ID | Criterion |
|---|---|
| G12b.1 | `f4_blink` on the HSI: the period in virtual time equals the cycle count of the compiled code |
| G12b.2 | The clock tree: `SystemCoreClock` and the baud rate on the TX pin follow the Crystal on the diagram and `PLLCFGR` exactly. A 25 MHz crystal with the PLL gives 168 MHz and 115200 baud; the same firmware with an 8 MHz crystal gives the wrong baud rate, as on the bench. HSE on without a Crystal: the chip waits and the log says why |
| G12b.3 | GPIO: output types, speed ignored, pull-ups and pull-downs, alternate functions by the table of the datasheet (a peripheral reaches a pin only through its AF number), BSRR, the 5 V tolerant inputs; EXTI on any port through SYSCFG |
| G12b.4 | USART1 to USART6: frames on the pins reach the Serial Port part and the terminal byte for byte, with `OVER8` on and off, and an RX interrupt echoes |
| G12b.5 | `f4_float` prints the checksum the board prints |
| G12b.6 | Resets: power-on, NRST from the circuit, software, the watchdogs, with their flags in `RCC_CSR`; BOOT0 high gives the message that the system bootloader is not simulated |
| G12b.7 | STM32CubeIDE debugs the firmware in the circuit through GDB: breakpoints, stepping, the FPU registers, the peripheral registers; the Register Viewer shows the chip's map (every register of the map read from the emulator by a test) |
| G12b.8 | Speed in the circuit, on the owner's laptop: `f4_blink` at 16 MHz at least 5 times real time; `f4_station` at 168 MHz, which sleeps between ticks, at least real time; a busy loop at 168 MHz recorded (expected below real time, as on the ESP32-C3), with the limit stated in the guide |
| G12b.9 | Board comparison with the owner's chip: `f4_clock` and `f4_float` with the same serial output byte for byte; blink period and baud rate within the crystal's tolerance; the five loops of `f4_timing` within 2 % after the calibration, with the measured numbers in the report |
| G12b.10 | Two runs of the same firmware with the same scripted inputs give identical traces (hash) |
| G12b.11 | Nothing changes for the STM32F103, the ESP32-C3 and the AVR chips: every ctest suite, `tools/examples_pass.py` and `tools/examples_gui_pass.py` pass unchanged; the engine protocol changes only by additions |

## Gate G12c — the peripherals students use

| ID | Criterion |
|---|---|
| G12c.1 | Timers: PWM frequency and duty exact in cycles on TIM1 (with complementary outputs and dead time), TIM2 and TIM5 with 32 bits, input capture, one-pulse mode, encoder mode, the basic timers as time bases and DAC triggers |
| G12c.2 | ADC: 12, 10, 8 and 6 bits, the result ⌊Vin·2ⁿ/VREF+⌋ ±1 LSB for a low-impedance source, scan and continuous modes, injected channels, a timer trigger, DMA, the temperature sensor and VREFINT. Labeled approximate until compared with the chip |
| G12c.3 | DAC: each channel drives its pin in the circuit with the voltage of its code (12 bits, VREF+), with the output buffer's impedance from the datasheet; a timer-triggered sine through DMA has the frequency the timer gives |
| G12c.4 | SPI master to the 74HC595 and MAX7219 parts; I2C master to the LM75 and 24C02 parts; both with the HAL |
| G12c.5 | DMA streams in direct mode: USART transmit and receive, the ADC in circular mode, memory to memory; half and full transfer interrupts |
| G12c.6 | RTC: the calendar counts on a 32.768 kHz Crystal and on the LSI, an alarm fires, the backup registers keep their values through a reset |
| G12c.7 | `f4_station` gives on the board the trace it gives in the simulator: serial lines byte for byte, PWM duty within one timer count, the DAC within 1 % of full scale |

## Outcomes (decision DE12)

- **G12a and G12b pass.** An ADR for the STM32F4 on the own emulator; the
  part ships as experimental until G12b.9, then G12c.
- **Only speed (G12a.4 or G12b.8) fails.** A week for performance, then
  re-review; the part may still ship with its measured speed stated.
- **G12a.1 or G12a.2 fails twice.** The experiment stops with its measured
  reasons, and the owner decides: a wrong FPU is worse than no F4.

## Risks

1. **FPU correctness.** A wrong rounding shows as a wrong number, not as a
   crash. G12a.2 and the checksum of G12b.5 are there for this.
2. **Speed at 168 MHz.** The core does about 100 MIPS today; firmware that
   never sleeps runs below real time. The guide says so, and examples use
   interrupts and WFI as good firmware does.
3. **Timing fidelity.** The flash wait states and the ART accelerator make
   the real chip's cycle counts depend on code placement. The model is
   calibrated on the owner's chip and labeled approximate outside what was
   measured.
4. **Alternate function table.** 82 pins by 16 functions from the
   datasheet: a typing mistake routes a peripheral to the wrong pin. The
   table is checked by a test against a second transcription.
5. **Size.** This is the largest chip so far. The stages are ordered so that
   each gate leaves something usable: after G12b, GPIO, serial and the FPU;
   after G12c, the peripherals of a typical project.

## References

- RM0090, STM32F405/415, STM32F407/417, STM32F427/437 and STM32F429/439
  reference manual (STMicroelectronics).
- DS8626, STM32F405xx and STM32F407xx datasheet.
- PM0214, STM32 Cortex-M4 MCUs and MPUs programming manual.
- ARMv7-M Architecture Reference Manual (Arm DDI 0403).
- Cortex-M4 Technical Reference Manual (Arm).
- ES0182, STM32F405/407xx and STM32F415/417xx device errata.
