# ESP32-C3 Wi-Fi web server (example 60)

`esp32c3_wifi_web.bin` is this folder's ESP-IDF project (`main/main.c`, MIT)
built with ESP-IDF v5.5.2 for the ESP32-C3 and merged into one flash image
from address 0:

```text
idf.py set-target esp32c3
idf.py build
idf.py merge-bin -o merged.bin
```

The firmware joins the Wi-Fi network `SimNodus` (WPA2-PSK, password
`simnodus-wifi`), prints its address on UART0 and serves `/`: the LED's
state, the chip temperature and links `/?led=1` and `/?led=0` that switch
the LED on GPIO8. On a real board it works the same with an access point of
that name and password.

The image also contains ESP-IDF (Apache-2.0), Espressif's Wi-Fi and PHY
libraries (Apache-2.0), lwIP (BSD-3-Clause), mbedTLS (Apache-2.0), newlib
(BSD-style) and FreeRTOS (MIT).
