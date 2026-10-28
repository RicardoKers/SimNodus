# ESP32-C3 blink

`blink.bin` is ESP-IDF v5.5.2's `examples/get-started/blink`, built for the
ESP32-C3 with `CONFIG_BLINK_LED_GPIO=y`, `CONFIG_BLINK_GPIO=8` and
`CONFIG_BLINK_PERIOD=500` (a plain GPIO instead of the board's WS2812 LED),
merged into one flash image from address 0. It is the same file as
`tests/experiments/esp32c3/firmware/blink.bin`, where its build is described.

License: Apache-2.0 (ESP-IDF and the example), Copyright (c) Espressif Systems
(Shanghai) Co. Ltd.; it contains newlib (BSD-style) and FreeRTOS (MIT).
