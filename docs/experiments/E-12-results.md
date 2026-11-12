# E-12: STM32F407VET6 on the own emulator — results

Plan and gates: [E-12-plan.md](E-12-plan.md). This report grows with the
work; a gate is marked met only with its numbers here.

Status on 2026-11-12: **G12a (the Cortex-M4F core) met on the host**, with
the limits listed under it. **The STM32F407 machine (SN-294) runs the first
four owned firmware from the command line**; it is not a part on the
diagram yet (SN-295) and nothing was compared with the owner's chip yet
(SN-296). G12c not started.

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

## Next

SN-295: the `stm32f407vet6_lqfp100` part with its pin model, the crystal
read from the diagram, NRST and BOOT0, GDB with the FPU registers, the
Register Viewer's map, examples and guides. Then SN-296: the owner runs
`f4_float`, `f4_clock` and `f4_blink` on the board
([how](../../examples/firmware/f4_common/README.md)) and the timing is
calibrated.
