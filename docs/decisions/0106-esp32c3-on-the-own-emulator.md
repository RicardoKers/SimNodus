# ADR 0106: The ESP32-C3 on SimNodus's own emulator, provisionally

Date: 2026-10-28. Status: **accepted, provisional** (decision DE1, owner,
2026-10-28), until the comparison with a real board (criterion G8b.1).

## Context

Experiment E-08 ([plan](../experiments/E-08-plan.md),
[results](../experiments/E-08-results.md)) asked whether SimNodus's own
emulator can run unmodified ESP32-C3 firmware. Gate G8a (the RV32IMC core)
is met. Gate G8b is met except one part of G8b.1: the console of the same
images on the owner's board, byte for byte. Unmodified ESP-IDF examples, an
Arduino-ESP32 sketch and the official MicroPython v1.29.0 image run from the
ROM, which is rebuilt from Espressif's ROM ELF and equals a ROM read from a
chip. The owner has no board yet and boards may take time.

## Decision

- The ESP32-C3 becomes a second microcontroller of SimNodus on the own
  emulator (`src/emu/riscv_cpu.*`, `src/emu/esp32c3.*`), integrated in the
  application as a board (SN-246), marked **experimental** in the
  application and the user guide until G8b.1 is met.
- The ROM is rebuilt at run time from `third_party/esp-rom-elfs`
  (Apache-2.0, decision DE0), shipped in the package.
- Firmware is a flash image (`.bin`, a merged image at offset 0); MicroPython
  is a firmware like any other.
- What the board comparison may still change (to be checked with the first
  board): misaligned accesses done by the core, the `Saved PC:` line after a
  CPU reset, the log's timestamps after light sleep, instruction timing
  (one instruction per cycle, labelled approximate).

## Alternatives

- *Wait for the boards before integrating*: the owner chose not to wait; the
  remaining risk is small (the ROM is identical, every official image tried
  runs) and differences found later are fixed in the machine, not in the
  integration.
- *The ESP32 (Xtensa) instead*: decided against on 2026-10-28 (E-08 plan).

## Consequences

- The runner, the pin model and `DeviceBus` stop being specific to the
  STM32F103 (an emulator machine interface); I2C, SPI and RMT on the C3 then
  reach SimNodus's chips at the pin level as on the STM32 (ADR 0102).
- The package grows by the ROM ELF (about 550 KB).
- Wi-Fi stays out (experiment E-09, SN-250).

## Revisit criteria

The comparison with a board (G8b.1); any official image that does not run;
a difference that the integration cannot hide behind the experimental mark.
