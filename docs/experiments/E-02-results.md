# E-02 result: standalone STM32 firmware and I/O

Date: 2026-08-31. Result: **passed for the bounded firmware/digital-I/O profile
below**. This is SN-012 evidence, not a production Renode adapter, complete STM32
model, electrical GPIO model, or coupled simulation.

## Question and outcome

Can the pinned Windows Renode/client pair boot an owned STM32F103C8 ELF and expose
usable timed output and injected input? Yes, within the explicit offline platform:
two fresh local runs passed both 100 us and 1000 us control profiles. Owned startup
restored initialized data, cleared BSS, configured SysTick, and reached the
firmware mailbox. PA0 toggled periodically; injected PA1 levels were sampled onto
PA2 and triggered EXTI1 acknowledgements on PA4.

The result also narrows the supported scope. GPIO configuration bits are stored,
but analog input, pulls, open-drain electrical release, slew rate, and RCC clock
gating are not modeled at hardware fidelity. ADC is absent from this profile.
GDB commands exist upstream, but runtime GDB/CubeIDE coordination was not tested.

## Exact environment and inputs

- Windows 11 `10.0.26200`, x64; Python 3.14.4; CMake 4.2.3-msvc3; MSVC
  19.51.36246.0 for the native C/C++ probe.
- Renode `1.16.1.19220`, commit `d66b0c2aa3d420408eccecfd1d3bab0fd702a6db`,
  .NET 8.0.10. Archive and selected package files retain the SN-010 fingerprints.
- SN-019's source-pinned Windows client and generated loopback-only server. Listener
  address, ephemeral port, process ownership, normal exit, and removal were checked
  in every profile.
- Local firmware compiler: GNU Tools for STM32 14.3.rel1.20251027-0700, GCC
  14.3.1 20250623; executable SHA-256
  `c8fcafea64559054bbfa87917182598892f81b41706b003c5a93fa7542355908`.
- Firmware ELF SHA-256
  `7895196a7e63134e5576f9bae2f4b124e7b0f1a3487891bea5a6ad56576b3689`.
  Two consecutive builds with the same compiler and recorded flags were identical.
- Platform SHA-256
  `e8c8e3b588a80573cf98fabdebc78afa499f8504a10bf01fb89c7ac47016837b`.
  It maps 64 KiB flash at `0x08000000` and zero, and 20 KiB SRAM at
  `0x20000000`. CPU throughput is 8 MIPS and SysTick is fixed at 8 MHz; neither
  value establishes instruction-cycle accuracy.
- Compact evidence: [E-02-summary.json](evidence/E-02-summary.json). The complete
  source, compiler flags, commands, hashes, and checks are in the
  [reproduction directory](../../tests/experiments/renode-stm32/README.md).

[Hosted run 33441607168](https://github.com/RicardoKers/SimNodus/actions/runs/33441607168)
passed two repetitions of both profiles on Windows Server 2025. The verified Arm
GNU Toolchain 14.3.Rel1 (Build arm-14.174) produced the same ELF SHA-256 as the
local CubeIDE-bundled compiler. Foundation, SN-019, and E-01 checks also passed on
the same revision. This establishes clean-runner reproducibility for the experiment,
not a packaged application or supported Windows release.

The firmware is freestanding C11 with its own vector table, reset handler, linker
map, mailbox, interrupt handlers, and direct register access. It links no HAL,
CMSIS, compiler runtime, or C library. The 64 KiB and 20 KiB bounds match the
[STM32F103C8 product specification](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8).
Register choices were checked against ST's [STM32F103 documentation index](https://www.st.com/en/microcontrollers-microprocessors/stm32f103/documentation.html).
Those references describe hardware; the results below describe the tested model.

## Measured behavior

Both fresh runs produced the same observations in both control profiles:

| Check | Observed result |
|---|---|
| Boot/memory | Initial time zero; vector/boot alias matched; stack `0x20005000`; poisoned initialized data restored to `0x13579bdf`; poisoned BSS cleared; no firmware fault |
| Time control | Every `RunFor` ended at the exact requested integer-microsecond time; each full profile ended at 23,900 us |
| Periodic output | 23 PA0 callbacks; successive timestamp gaps were 998-1000 us, below the predeclared 100 us fine-profile tolerance |
| Persistent input | High at 2500 us and low at 5500 us each produced one EXTI interrupt; PA4 acknowledged at those timestamps; PA2 changed at the next 1 ms sample |
| Repeated level | Repeating low produced no additional interrupt |
| Short pulse | High at 7600 us and low at 7620 us produced two EXTI interrupts while PA2 remained low, demonstrating interrupt observation independently of 1 ms polling |
| Same-time edges | High then low before CPU execution produced one additional interrupt with final low state; the two edges were not independently preserved |
| Callback boundary | Every captured callback arrived while native `RunFor` was active and its microsecond timestamp fell within that request's interval |
| Input/argument failures | Missing `adc1` and GPIO output number 16 returned recoverable command failures |
| Shutdown/exposure | Only the expected `127.0.0.1` listener owned by Renode appeared; each process exited normally and its listener disappeared |

The 100 us profile initially reported PA0 at 1000 us; the 1000 us profile reported
it at 999 us. Later events converged on offsets of up to 3 us. This is reproducible
within each profile and small relative to the declared target, but it means a
callback timestamp must not be presented as exact instruction execution time.
It is Renode master virtual time at event reporting.

## GPIO and peripheral audit

The pinned `STM32F1GPIOPort` source implements CRL/CRH mode fields, IDR, ODR,
BSRR/BRR, input acceptance, and alternate-output selection. Its mode-dependent
input guard worked: external input was ignored while PA3 was configured as an
output. Its observed mode behavior was otherwise digital:

| PA3 mode | External low/high observation | Interpretation |
|---|---|---|
| Floating input | 0 / 1 | Boolean input works |
| Pull-down / pull-up configuration | 0 / 1 in both cases | API has no high-impedance release; pull behavior was not established |
| Analog input | 0 / 1 | Analog mode still accepted Boolean input; not hardware-faithful analog isolation |
| Push-pull output | 1 / 1 after driving high | Input injection rejected as expected for output |
| Open-drain output | 1 / 1 after driving high | Behaved as a driven Boolean high; no electrical release/open-drain claim |
| Alternate push-pull | 0 / 0 without a selected source | Mode connection existed, but timer/AF routing was not validated |

The firmware wrote the APB2 enable register, then the host stored zero in the
profile's RCC region. SysTick advanced from 21 to 23 and GPIO continued operating.
That region is deliberately named `rccRegisterStorage`: it supplies observable
register storage only. It does not prove reset values, HSI/HSE/PLL, prescalers,
clock gates, or frequency propagation.

The inspected generic `STM32_ADC` is explicitly partial and was neither selected
nor instantiated. The offline C8 profile contains no `IADC`, so the external ADC
lookup correctly failed. Although the real part has two 12-bit ADCs, E-04 still
needs an appropriate model or focused extension, channel/voltage semantics, and
conversion tests.

The pinned GDB implementation provides continue, single-step, halt, register, and
memory commands. Its ordinary port constructor uses the same socket-provider path
whose default exposure is not loopback-only. No GDB listener was opened here.
SN-016 must establish safe exposure and coordinate debugger advancement with the
simulation time owner before CubeIDE support is claimed.

## Interpretation and limits

E-02 passes because real owned firmware booted and the required bounded digital
output/input/interrupt flow was reproduced with recorded timestamps. The 20 us
pulse result does not establish a minimum supported pulse width. The same-time
edge result shows that an input supplied before CPU execution can collapse at the
pending-interrupt level. Neither result proves safe analog feedback, event
prediction, rollback, or an I/O-boundary stop.

The platform omits most peripherals and board wiring. It does not model the Blue
Pill LED, oscillator, power, boot pins, USB pull-up, clones, or electrical nets.
Pulls and open-drain require an external electrical domain plus an explicit logic
boundary. Timer/PWM, UART, SPI/I2C, DMA, watchdogs, flash programming, ADC, and GDB
remain unvalidated. Paths without whitespace and isolated runtime temporary files
remain experiment constraints.

No upstream files are copied into the profile. Fourteen exact source files were
downloaded only for read-only audit and verified by SHA-256; their URLs and hashes
are in `audit-sources.json`. The compiler is a development input and is not
redistributed. Source and package licenses remain with their owners. No upstream
issue or contribution was submitted.
