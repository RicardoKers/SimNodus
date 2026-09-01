# E-03 GPIO/RC coupling and feedback experiment

This Windows-only experiment executes the [predeclared SN-013 contract](../../../docs/architecture/TEMPORAL_CAPABILITY_PROFILE.md). It uses the real pinned ngspice DLL, the real pinned Renode process and native client, an owned STM32F103C8 firmware image, and an owned 1 kOhm / 1 uF fixture. It is not a production scheduler or adapter.

The firmware drives PA0 HIGH after 1 ms and LOW after 4 ms. Replay first records those callbacks and supplies the known schedule to ngspice. Sampled profiles then advance Renode to a boundary, expose the PA0 schedule to ngspice, locate accepted 0.70/0.30 VDD crossings, and apply the resolved level to PA1. EXTI1 acknowledges the result on PA4. The ordering deliberately records late feedback instead of concealing it.

## Prerequisites

Complete the pinned backend setup, SN-019 preparation, and E-02 platform setup first. Use paths without whitespace, as required by the bounded Renode profile. Select an existing ARM GCC explicitly; the script does not download a compiler.

## Build

```powershell
python tests/experiments/coupling/build_firmware.py `
  --toolchain C:/path/to/arm-none-eabi/bin `
  --output build/sn014/firmware

cmake -S tests/experiments/coupling -B build/sn014/native -A x64 `
  -DPREPARED_ROOT=D:/path/to/SimNodus/build/sn019/generated `
  -DFIRMWARE_ROOT=D:/path/to/SimNodus/build/sn014/firmware `
  -DNGSPICE_ROOT=D:/path/to/SimNodus/build/deps/ngspice/Spice64_dll
cmake --build build/sn014/native --config Debug --target e03_probe
```

## Run

Use `--quick` only while developing the host. It does not satisfy E-03.

```powershell
python tests/experiments/coupling/run.py --quick
python tests/experiments/coupling/run.py --output build/sn014/final
```

The full run executes every replay, sampled, boundary, digital-pulse, same-time, late-input, failure, and recovery profile three times from fresh processes. Raw CSV/logs stay under the ignored output directory. `summary.json` contains hashes and the compact analysis. A committed report and sanitized summary are produced only after the full run.

The analyzer enforces the fixed 16.5 mV voltage limit, 2 us crossing limit, `Q + 2 us` sampled-delay limit, qualifying-pulse rule, exact T-1/T/T+1 boundary placement, repeated discrete ordering, and no-commit behavior after an injected backend termination. Passing sampled checks earns only an approximate classification unless the trace independently proves that Renode never passed the effective event before application.
