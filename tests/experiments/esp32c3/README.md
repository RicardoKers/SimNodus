# ESP32-C3 firmware for the emulator tests (E-08)

Unmodified flash images that `emu-esp32c3` (`tests/unit/esp32c3_tests.cpp`)
boots on SimNodus's ESP32-C3 machine, for gate G8b of the
[E-08 plan](../../../docs/experiments/E-08-plan.md). Each goes into the flash
at offset 0.

| File | What | SHA-256 | Licence |
|---|---|---|---|
| `firmware/hello_world.bin` | ESP-IDF v5.5.2 `examples/get-started/hello_world`, target esp32c3, default configuration; `idf.py merge-bin` of the bootloader, the partition table and the application (flash header: DIO, 80 MHz, 2 MB) | `0850984fddd15dd6ed4bf3b9aa3e5bd80fe283f1018dcb3c1d210342dbd5bebf` | ESP-IDF and the example: Apache-2.0 (Espressif) |
| `firmware/ESP32_GENERIC_C3-20260824-v1.29.0.bin` | The official MicroPython v1.29.0 release for generic ESP32-C3 boards, from https://micropython.org/download/ESP32_GENERIC_C3/ (built with ESP-IDF v5.5.2) | `bf72ed9eb88ad3a8f49d02c3d371f9ea34c90a8a303aeb9e324d8efd4a2a655a` | MicroPython: MIT; ESP-IDF and Espressif's Wi-Fi and Bluetooth libraries inside it: Apache-2.0 |

Rebuilding `hello_world.bin` (ESP-IDF v5.5.2 installed with `install.bat esp32c3`):

```text
idf.py -C <esp-idf>/examples/get-started/hello_world set-target esp32c3
idf.py -C <esp-idf>/examples/get-started/hello_world build merge-bin -o merged.bin
```

The build date and the project's git version are inside the image, so a
rebuild gives a different SHA-256 with the same behaviour.

Running one by hand (the ROM ELF comes from `third_party/esp-rom-elfs`):

```text
build/dev/apps/simnodus-emu/simnodus-emu --esp32c3 tests/experiments/esp32c3/firmware/hello_world.bin --ms 3000
build/dev/apps/simnodus-emu/simnodus-emu --esp32c3 tests/experiments/esp32c3/firmware/ESP32_GENERIC_C3-20260824-v1.29.0.bin --ms 1000 --usb "print(6*7)\r@500"
```

`--elf file.elf` names functions in diagnostics, `--trace-io` prints every
peripheral register access with the function at the PC, `--flash-out file`
saves the flash at the end (pass it back as the image to continue).
