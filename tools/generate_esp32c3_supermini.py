#!/usr/bin/env python3
# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Writes resources/library/mcu/esp32c3_supermini.json: the ESP32-C3 SuperMini board.

The board seen from above with the USB connector at the top, as its pinout
drawings show it: GPIO5 to GPIO21 down the left side, 5V, G, 3V3 and GPIO4 to
GPIO0 down the right side. The chip is the DevKitM-1's (the same emulator);
the board differs in its pins, its LED (GPIO8, lit while low), and in having
only the chip's own USB Serial/JTAG on its connector (no USB-serial bridge:
UART0 is on the pins GPIO21 and GPIO20 alone).

Usage: python tools/generate_esp32c3_supermini.py
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "resources" / "library" / "mcu" / "esp32c3_supermini.json"

LEFT = ["GPIO5", "GPIO6", "GPIO7", "GPIO8", "GPIO9", "GPIO10", "GPIO20", "GPIO21"]
RIGHT = ["5V", "G1", "3V3", "GPIO4", "GPIO3", "GPIO2", "GPIO1", "GPIO0"]
TOP = -6  # y of the first pin; 2 grid units between pins


def label(name):
    return {"G1": "G"}.get(name, name.replace("GPIO", "IO"))


def build():
    graphics = [
        {"rect": [-7, -10, 14, 20]},
        {"rect": [-2.5, -10, 5, 2.5]},
        {"text": "USB", "at": [0, -8.7], "size": 0.8},
        {"text": "ESP32-C3", "at": [0, -4.6], "size": 1.0},
        {"text": "SuperMini", "at": [0, -3.0], "size": 1.0},
        {"rect": [-2, 0, 4, 2]},
        {"text": "RESET", "at": [0, 2.8], "size": 0.7},
        {"text": "LED IO8", "at": [0, 7.9], "size": 0.7},
    ]
    pins = []
    for side, names in ((-1, LEFT), (1, RIGHT)):
        for i, name in enumerate(names):
            y = TOP + 2 * i
            graphics.append({"line": [side * 9, y, side * 7, y]} if side < 0 else {"line": [7, y, 9, y]})
            graphics.append({"text": label(name), "at": [side * 5.3, y], "size": 0.8})
            pins.append({"name": name, "at": [side * 9, y], "dir": "left" if side < 0 else "right"})
    gpios = [n for n in LEFT + RIGHT if n.startswith("GPIO")]
    spice = [
        "V_{ref}_reg reg_{ref} {node:G1} DC 3.3",
        "R_{ref}_reg reg_{ref} {node:3V3} 0.1",
        "R_{ref}_5v {node:5V} {node:G1} 10Meg",
        # The strapping pull-ups of the board: GPIO9 (BOOT) and GPIO8 (with the LED).
        "R_{ref}_boot {node:GPIO9} {node:3V3} 10k",
        "R_{ref}_strap8 {node:GPIO8} {node:3V3} 10k",
    ]
    spice += ["R_{ref}_%s {node:%s} {node:G1} 100Meg" % (g.lower(), g) for g in gpios]
    spice += ["B_{ref}_%s {node:G1} {node:%s} I=V({ext:%s_i})-V({ext:%s_g})*(V({node:%s})-V({node:G1}))"
              % (g.lower(), g, g.lower(), g.lower(), g) for g in gpios]
    return {
        "type": "esp32c3_supermini",
        "name": "ESP32-C3 SuperMini (experimental)",
        "category": "Microcontrollers",
        "prefix": "U",
        "description": "ESP32-C3 SuperMini board running your firmware (flash image, .bin: ESP-IDF, Arduino or "
                       "MicroPython) in the SimNodus emulator; its LED is on GPIO8 and lights while the pin is low; "
                       "experimental until compared with a real board",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-7, -13],
        "value_offset": [-2, -13],
        "value_parameter": "firmware",
        "parameters": [
            {"name": "firmware", "label": "Firmware", "kind": "file", "filter": "*.bin", "default": "",
             "description": "Flash image for the ESP32-C3, written from address 0 (a merged .bin of ESP-IDF or "
                            "Arduino, or MicroPython's firmware .bin), or an application .bin with its build's "
                            "flash_args beside it; stored relative to the project"},
            {"name": "usb_link", "label": "USB Serial/JTAG to the computer", "kind": "text", "host_link": True,
             "default": "",
             "description": "The board's USB connector (the chip's own USB Serial/JTAG) linked to the computer: a "
                            "serial port (one end of a virtual port pair such as com0com), tcp:4001 for programs "
                            "that speak RFC 2217 (esptool, mpremote), or empty for none. DTR and RTS reset the chip "
                            "and select download mode as the USB Serial/JTAG does"},
            {"name": "chip_temperature", "label": "Chip temperature", "unit": "°C", "default": "25",
             "min": "-40", "max": "125", "live": True,
             "description": "Temperature of the chip that its temperature sensor measures; change it while running "
                            "to test the firmware (-40 to 125 °C)"},
        ],
        "mcu": {
            "chip": "esp32c3",
            "firmware_parameter": "firmware",
            "led": {"pin": "GPIO8", "active_low": True, "at": [0, 6], "radius": 0.6},
            "reset_button": [-2, 0, 4, 2],
            "pin_model": {"supply": 3.3, "output_resistance": 33, "pull_resistance": 45000,
                          "input_high": 2.475, "input_low": 0.825},
            "uarts": [
                {"name": "usb", "label": "USB Serial/JTAG (the USB connector)", "tx_pin": "", "rx_pin": "",
                 "link_parameter": "usb_link", "reset_circuit": "usb_jtag"},
                {"name": "uart0", "label": "UART0 (GPIO21 TX, GPIO20 RX)", "tx_pin": "GPIO21", "rx_pin": "GPIO20"},
            ],
        },
        "spice": spice,
    }


def main():
    OUT.write_text(json.dumps(build(), indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print("wrote", OUT.relative_to(ROOT))


if __name__ == "__main__":
    main()
