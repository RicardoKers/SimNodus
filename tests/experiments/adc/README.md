# E-04 STM32F103 ADC path experiment

This directory owns SN-015's bounded Windows experiment. It does not contain a
production adapter or claim complete STM32F103 ADC fidelity.

## Audited model choice

Renode 1.16.1.19220 does not instantiate an ADC in the selected offline C8
platform. Its pinned infrastructure revision contains `STM32_ADC`, but that
model is not suitable for the E-04 path:

- its regular software-start bit is 30, while the STM32F103 register definition
  places `SWSTART` at bit 22;
- it exposes 19 queued raw-code channels through `FeedSample` and does not
  implement Renode's external `IADC` voltage interface;
- feeding an already quantized code through that route would leave voltage
  units outside the backend contract and invite double quantization.

E-04 therefore uses an owned, focused `SimNodusSTM32F103ADC` experiment extension. Its
register subset is checked against the pinned official ST CMSIS and HAL sources.
The extension implements Renode's pinned `IADC` contract directly, so the native
client passes voltage once in integer microvolts and the peripheral performs the
only voltage-to-code conversion.

This is a deliberate experiment model, not an upstream Renode model or an
assertion that every STM32F103 ADC feature works.

## Predeclared profile

- ADC1 base address: `0x40012400`.
- External channels: 0 through 15; channel 0 maps to PA0/ADC1_IN0 and channel 1
  maps to PA1/ADC1_IN1 for the checked board profile. Internal temperature and
  VREF channels are not exposed.
- External API unit: unsigned integer microvolts, preserved exactly by
  `SetADCValue` / `GetADCValue` before conversion.
- Reference: fixed 3,300,000 uV. Dynamic VDDA/VREF behavior is unsupported.
- Resolution: 12 bits, right aligned for the firmware path.
- Quantization: `min(4095, floor(VIN_uV * 4096 / 3300000))`. Zero maps to zero;
  values at or above VREF saturate to 4095. The unsigned API cannot express a
  negative input.
- ADC clock: fixed 1 MHz. The eight STM32F103 sampling selections produce total
  conversion durations of 14, 20, 26, 41, 54, 68, 84, and 252 us after adding
  the documented 12.5 conversion cycles. E-04's normal firmware path uses the
  1.5-cycle selection and therefore 14 us.
- Sample instant: the voltage is captured when an enabled ADC accepts
  `SWSTART`; a later voltage update cannot change the in-flight result. EOC is
  raised at the declared conversion endpoint. Reading DR clears EOC.
- Operating mode: one regular software-triggered conversion, polling EOC, and
  one rank. Calibration is treated as an instantaneous no-op. Scan, continuous,
  injected, external-trigger, interrupt, DMA, watchdog, dual ADC, and electrical
  acquisition effects are unsupported.

## Cases and gates

Run the complete profile in three fresh Renode processes. Each process must use
the verified loopback-only server and owned firmware, and must exit normally.
Repeated discrete results must match exactly.

1. Verify that ADC lookup succeeds, reports 16 external channels, and round-trips
   0, 825000, 1650000, 2475000, 3300000, and 3400000 uV without API quantization.
2. Through firmware register access, convert 0%, 25%, 50%, 75%, and 100% of VREF.
   Each result must be within one code of the declared quantization rule.
3. Check 805 and 806 uV across the first code boundary, 3299999 uV immediately
   below VREF, and 3400000 uV above VREF. Saturation and every boundary result
   must be within one code of the declared rule.
4. Set channel 0 to 25% and channel 1 to 75%, select each through SQR3, and require
   firmware to return the corresponding code. This is the bounded channel-mapping
   evidence.
5. Apply a 0-to-3.3 V stepped ramp in 33000 uV increments (101 samples). Require
   monotonic firmware codes, both endpoints, no missing sample, and at most one
   code of error at every point.
6. Trigger a conversion directly through the audited registers. Require EOC to
   remain clear through 13 us and become set at 14 us. Change 25% to 75% after
   1 us of that conversion and require the completed code to retain the 25%
   start sample.
7. Attempt software start while disabled and require no EOC or result after
   20 us. Reject external channel indices -1 and 16 without changing a valid
   channel value or advancing virtual time.

The committed report must retain exact source, executable, extension, firmware,
platform, and backend hashes; requested and observed virtual times; inputs,
codes, errors, process supervision, and unsupported modes. Tolerances must not be
relaxed after observing results.

## Reference sources

- [Pinned Renode STM32 ADC model](https://github.com/renode/renode-infrastructure/blob/add012af003a0f620d3da52828262676f374d121/src/Emulator/Peripherals/Peripherals/Analog/STM32_ADC.cs)
- [Pinned Renode IADC voltage contract](https://github.com/renode/renode-infrastructure/blob/add012af003a0f620d3da52828262676f374d121/src/Emulator/Main/Peripherals/Sensor/IADC.cs)
- [Pinned ST STM32F103xB register definitions](https://github.com/STMicroelectronics/cmsis_device_f1/blob/c8e9a4a4f16b6d2cb2a2083cbe5161025280fb22/Include/stm32f103xb.h)
- [Pinned ST STM32F1 HAL ADC contract](https://github.com/STMicroelectronics/stm32f1xx-hal-driver/blob/baeff0a8dcb23c72012170a0978254a238f1f980/Inc/stm32f1xx_hal_adc.h)

The sources above are audit inputs and are not redistributed by this directory.

## Reproduce on Windows

Use the exact Renode and Arm compiler packages from the repository manifests.
SN-019's preparation step creates the source-pinned native client and
loopback-only server under ignored build output.

```powershell
python tests/experiments/renode-client/prepare.py --download
python tests/experiments/adc/audit.py --download
python tests/experiments/adc/build_firmware.py --toolchain <arm-toolchain-bin>
cmake -S tests/experiments/adc -B build/sn015/native -A x64 `
  -DPREPARED_ROOT=<absolute-prepared-root> `
  -DFIRMWARE_ROOT=<absolute-firmware-root>
cmake --build build/sn015/native --config Debug --target e04_probe
python tests/experiments/adc/run.py --output build/sn015/results
python tests/experiments/adc/summarize.py `
  build/sn015/results/summary.json build/sn015/compact.json
python tests/experiments/adc/plot.py `
  build/sn015/results/summary.json build/sn015/e04-adc.svg
```

Replace the angle-bracket placeholders with machine-local absolute paths; do
not commit them. The runner's defaults use `build/deps/renode`,
`build/sn019/generated`, `build/sn015/firmware`, and
`build/sn015/native/Debug/e04_probe.exe`. The dedicated Windows workflow
downloads and verifies the same packages and executes this complete profile.
