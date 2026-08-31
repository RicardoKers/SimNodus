# E-02: owned STM32 firmware and I/O

This opt-in SN-012 experiment uses the pinned SN-019 native Windows client and
loopback extension. It is not a production adapter or a complete Blue Pill model.
The acceptance criteria below were written before running the firmware.

## Profile and acceptance

- Own startup, linker script, and freestanding firmware; no HAL, CMSIS, vendor
  firmware, SVD download, or runtime library. Record sources, ELF, compiler, client,
  platform, and backend fingerprints. Verify a second build produces the same ELF
  with the same compiler and flags.
- Cortex-M3, 64 KiB flash at `0x08000000` with a boot alias at zero, 20 KiB SRAM
  at `0x20000000`. Explicit vector table and stack; prove initialized data and
  cleared BSS via firmware-owned status. The host must not write status results.
- Fixed 8 MHz SysTick and 8 MIPS CPU throughput. MIPS is a simulation setting,
  not cycle accuracy. RCC is explicitly **register storage only**; it does not
  gate peripherals or propagate clock changes. No PLL/HSE/clock-tree claim.
- PA0 toggles from a 1 ms SysTick interrupt. PA1 is injected externally; firmware
  samples it into PA2 and counts EXTI1 interrupts. PA4 acknowledges EXTI input.
  PA3 is reserved for mode probes. No onboard LED or voltage/current model.
- Real native callbacks must copy events before returning, never throw through
  the C API, and record the requested interval and whether delivery occurs inside
  `RunFor`. Never call the control API reentrantly from a callback.
- Require exact requested/observed integer-microsecond advancement; rising and
  falling output edges, input visible to firmware, both EXTI polarities, stable
  replay in two fresh runs. Expected 1 ms GPIO spacing has a predeclared 100 us
  observation tolerance for the fine-quantum profile; record all deviations.
- Compare fine (100 us) and coarse (1000 us) synchronization quanta. Record
  timestamp granularity and callback delivery; do not infer instruction-level
  event time or safe cross-engine feedback from the master-time timestamp.
- Additional input gates, declared after the initial boot run: a repeated level
  must not create an interrupt; a 20 us pulse must reach EXTI on both edges even
  if 1 ms sampling misses it. Record, without assuming two interrupts, two input
  edges delivered at the same virtual time before allowing CPU execution.
- Audit floating input, pulls, analog mode, push-pull, open-drain, alternate
  function selection, and disabled RCC gates. Missing electrical behavior is an
  explicit coverage finding, not silently accepted hardware equivalence.
- Audit ADC model/interface availability and GDB start/continue/step semantics
  against pinned source. Runtime ADC and GDB/CubeIDE validation remain E-04/E-05;
  do not open the original all-interface GDB server for this audit.
- Verify actual loopback listener address/PID, bounded subprocess execution,
  ordinary shutdown, and listener removal. Run SN-019 regression if its recipe
  changes. No ngspice coupling is performed here.

## Reproduction

Prerequisites are the pinned Renode archive and prepared SN-019 client, an existing
Arm GCC `tools/bin` directory, CMake with MSVC x64, and Python 3.10 or newer. The
local inventory uses STM32CubeIDE's GNU Tools for STM32. CI explicitly downloads
the checksum-pinned Arm archive described in `ci-toolchain.json`; that toolchain is
not an application dependency or redistributable SimNodus asset.

```powershell
python tests/experiments/renode-client/prepare.py
python tests/experiments/renode-stm32/audit.py
python tests/experiments/renode-stm32/build_firmware.py --toolchain <arm-tools-bin>
$prepared = (Resolve-Path build/sn019/generated).Path
$firmware = (Resolve-Path build/sn012/firmware).Path
cmake -S tests/experiments/renode-stm32 -B build/sn012/native -A x64 "-DPREPARED_ROOT=$prepared" "-DFIRMWARE_ROOT=$firmware"
cmake --build build/sn012/native --config Debug
python tests/experiments/renode-stm32/run.py --output build/sn012/results
```

Omit `--download` to reuse already verified audit/control source files offline. The
GitHub workflow uses `--download` only after an explicit invocation. It downloads
the pinned backend, compiler, and small audit sources; ordinary configuration,
firmware building, and running never download anything.

Generated firmware, downloaded audit sources, native binaries, raw logs, isolated
runtime configuration, and results belong under ignored `build/sn012`. Opening a
circuit must never invoke this experiment automatically. See the [measured result](../../../docs/experiments/E-02-results.md).
