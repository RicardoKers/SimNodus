# ESP32-C3 firmware for the emulator tests (E-08)

Unmodified flash images that `emu-esp32c3` (`tests/unit/esp32c3_tests.cpp`)
boots on SimNodus's ESP32-C3 machine, for gate G8b of the
[E-08 plan](../../../docs/experiments/E-08-plan.md). Each goes into the flash
at offset 0.

| File | What | SHA-256 | Licence |
|---|---|---|---|
| `firmware/hello_world.bin` | ESP-IDF v5.5.2 `examples/get-started/hello_world`, default configuration | `0850984fddd15dd6ed4bf3b9aa3e5bd80fe283f1018dcb3c1d210342dbd5bebf` | Apache-2.0 (Espressif) |
| `firmware/blink.bin` | ESP-IDF v5.5.2 `examples/get-started/blink` with `sdkconfig.defaults.esp32c3` replaced by `CONFIG_BLINK_LED_GPIO=y`, `CONFIG_BLINK_GPIO=8`, `CONFIG_BLINK_PERIOD=500` (a plain GPIO instead of the WS2812 LED) | `e1fb31e838c63fa63a64b0badc8a9b6ea842a4f9cc17a1ede034db2c57d7d580` | Apache-2.0 (Espressif) |
| `firmware/esp_timer.bin` | ESP-IDF v5.5.2 `examples/system/esp_timer`, default configuration (periodic and one-shot timers, light sleep) | `c52b5b0ae5e88b6b39b708e41368bdd754d62e5e8cbc644662b59cfe4216e449` | Apache-2.0 (Espressif) |
| `firmware/real_time_stats.bin` | ESP-IDF v5.5.2 `examples/system/freertos/real_time_stats`, default configuration (FreeRTOS tasks, run-time statistics) | `d1c0a20c5f16445c36637a99998f0cd83f8c966ad053cf061b04af82a3a0ccc8` | Apache-2.0 (Espressif) |
| `firmware/faults.bin` | `apps/faults` (ours) built with ESP-IDF v5.5.2: a console character picks an illegal instruction, a load from nothing, or `abort()` | `a2a828dcc114232a9a617fdce9a6e486abc56cd66ab987f4e21f82525d360a76` | MIT (the application), Apache-2.0 (ESP-IDF) |
| `firmware/arduino_serial_blink.bin` | `arduino_serial_blink.ino` built by `arduino-cli` 1.5.1 with Arduino-ESP32 3.3.12, board `esp32:esp32:esp32c3` (ESP32C3 Dev Module) with its default options; the IDE's `.merged.bin` without its 0xFF fill (bootloader 0x0, partitions 0x8000, `boot_app0` 0xE000, application 0x10000) | `284796f055bea2d80f9ae068323122c9d383f588bf80936e399a7c81d2a84757` | MIT (the sketch), LGPL-2.1 (Arduino-ESP32 core), Apache-2.0 (ESP-IDF inside it) |
| `firmware/ESP32_GENERIC_C3-20260824-v1.29.0.bin` | The official MicroPython v1.29.0 release for generic ESP32-C3 boards, from https://micropython.org/download/ESP32_GENERIC_C3/ (built with ESP-IDF v5.5.2) | `bf72ed9eb88ad3a8f49d02c3d371f9ea34c90a8a303aeb9e324d8efd4a2a655a` | MicroPython: MIT; ESP-IDF and Espressif's Wi-Fi and Bluetooth libraries inside it: Apache-2.0 |

The ESP-IDF images are `idf.py merge-bin` of the bootloader, the partition
table and the application (flash header: DIO, 80 MHz, 2 MB). Rebuilding one
(ESP-IDF v5.5.2 installed with `install.bat esp32c3`):

```text
idf.py -C <project> set-target esp32c3
idf.py -C <project> build merge-bin -o merged.bin
arduino-cli compile --fqbn esp32:esp32:esp32c3 --export-binaries <sketch-folder>
```

The build date and the project's version are inside each image, so a rebuild
gives a different SHA-256 with the same behaviour.

The Arduino-ESP32 image is test material only and is not shipped with
SimNodus. It contains the Arduino-ESP32 core, LGPL-2.1: the core's source is
the public tag https://github.com/espressif/arduino-esp32/tree/3.3.12, and the
sketch (`arduino_serial_blink.ino`, MIT) with the command above rebuilds and
relinks the image (LGPL-2.1 section 6).

Running one by hand (the ROM ELF comes from `third_party/esp-rom-elfs`):

```text
build/dev/apps/simnodus-emu/simnodus-emu --esp32c3 tests/experiments/esp32c3/firmware/blink.bin --ms 3000
build/dev/apps/simnodus-emu/simnodus-emu --esp32c3 tests/experiments/esp32c3/firmware/ESP32_GENERIC_C3-20260824-v1.29.0.bin --ms 1000 --usb "print(6*7)\r@500"
```

`--elf file.elf` names functions in diagnostics, `--usb`, `--uart0` and
`--uart1` type text (`\r` and `\n` allowed) at a time in ms, `--input
GPIO=0|1@ms` sets a pad's level from outside, `--trace-io` prints every
peripheral register access with the function at the PC, `--flash-out file`
saves the flash at the end (pass it back as the image to continue).
