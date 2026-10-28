# ESP32-C3 I2C (example 46)

`esp32c3_i2c.bin` is this folder's ESP-IDF project (`main/main.c`, MIT) built
with ESP-IDF v5.5.2 for the ESP32-C3 and merged into one flash image from
address 0:

```text
idf.py set-target esp32c3
idf.py build
idf.py merge-bin -o merged.bin
```

The image also contains ESP-IDF (Apache-2.0), newlib (BSD-style) and FreeRTOS
(MIT).
