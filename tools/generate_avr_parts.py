"""Generate the AVR chip parts (experiment E-10, SN-258, SN-260) and the Crystal part.

resources/library/mcu/atmega8_pdip28.json, atmega8a_pdip28.json,
atmega328_pdip28.json and atmega328p_pdip28.json: the bare chips in PDIP-28, pins named as on the chip (PB0..PD7; PC6 is RESET until
RSTDISBL is programmed, PB6/PB7 are XTAL1/XTAL2 when the fuses select a
crystal), VCC, GND, AVCC and AREF. Each GPIO drives the circuit through the
runner's pin model (5 V; the supply voltage on VCC is not read yet: SN-259).
resources/library/mcu/atmega32_pdip40.json and atmega32a_pdip40.json (SN-263):
the ATmega32 and ATmega32A in PDIP-40, with ports A to D, RESET, XTAL1 and
XTAL2 as pins of their own.
resources/library/mcu/arduino_uno.json: the Arduino UNO R3 (SN-261), an
ATmega328P with the board's fuses, its 16 MHz crystal and Optiboot in the
boot section; pins named as on the headers (D0..D13, A0..A5), the USB
connector with DTR auto-reset.
resources/library/passive/crystal.json: a crystal or ceramic resonator whose
frequency is the chip's clock when it sits between XTAL1 and XTAL2; its
oscillation is not simulated.

    python tools/generate_avr_parts.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# PDIP-28 pinout of the ATmega8/8A: (pin number, name, label).
PINOUT = [
    (1, "PC6", "PC6/RESET"), (2, "PD0", "PD0/RXD"), (3, "PD1", "PD1/TXD"), (4, "PD2", "PD2/INT0"),
    (5, "PD3", "PD3/INT1"), (6, "PD4", "PD4/T0"), (7, "VCC", "VCC"), (8, "GND", "GND"),
    (9, "PB6", "PB6/XTAL1"), (10, "PB7", "PB7/XTAL2"), (11, "PD5", "PD5/T1"), (12, "PD6", "PD6/AIN0"),
    (13, "PD7", "PD7/AIN1"), (14, "PB0", "PB0/ICP1"), (15, "PB1", "PB1/OC1A"), (16, "PB2", "PB2/OC1B"),
    (17, "PB3", "PB3/OC2"), (18, "PB4", "PB4/MISO"), (19, "PB5", "PB5/SCK"), (20, "AVCC", "AVCC"),
    (21, "AREF", "AREF"), (22, "GND2", "GND"), (23, "PC0", "PC0/ADC0"), (24, "PC1", "PC1/ADC1"),
    (25, "PC2", "PC2/ADC2"), (26, "PC3", "PC3/ADC3"), (27, "PC4", "PC4/SDA"), (28, "PC5", "PC5/SCL"),
]
GPIO = [name for _, name, _ in PINOUT if name.startswith("P")]
# The ATmega328/328P: the same pins, with Timer0's and Timer2's two outputs each.
LABELS_328 = {"PD3": "PD3/INT1/OC2B", "PD5": "PD5/T1/OC0B", "PD6": "PD6/AIN0/OC0A", "PB3": "PB3/OC2A"}


def chip(chip_id, title, description, m328=False):
    graphics = [{"rect": [-7, -15, 14, 30]}, {"arc": [0, -15, 0.8, 180, 180]},
                {"text": title, "at": [0, 0], "size": 0.9}]
    pins = []
    for number, name, label in PINOUT:
        if m328:
            label = LABELS_328.get(name, label)
        left = number <= 14
        y = -13 + 2 * ((number - 1) if left else (28 - number))
        x_end, x_body = (-9, -7) if left else (9, 7)
        graphics.append({"line": [x_end, y, x_body, y]})
        graphics.append({"text": label, "at": [-3.9 if left else 3.9, y], "size": 0.55})
        graphics.append({"text": str(number), "at": [-8.0 if left else 8.0, y - 0.5], "size": 0.45})
        pins.append({"name": name, "at": [x_end, y], "dir": "left" if left else "right"})
    spice = ["R_{ref}_gnd2 {node:GND} {node:GND2} 1m",
             # Supply current of the core and the oscillator (about 2 mA at 5 V).
             "R_{ref}_vcc {node:VCC} {node:GND} 2.5k",
             "R_{ref}_avcc {node:AVCC} {node:GND} 100k",
             "R_{ref}_aref {node:AREF} {node:GND} 32k"]  # AREF's input resistance (typical)
    for name in GPIO:
        low = name.lower()
        spice.append(f"R_{{ref}}_{low} {{node:{name}}} {{node:GND}} 100Meg")
        spice.append(f"B_{{ref}}_{low} {{node:GND}} {{node:{name}}} "
                     f"I=V({{ext:{low}_i}})-V({{ext:{low}_g}})*(V({{node:{name}}})-V({{node:GND}}))")
    if m328:
        fuses = [
            {"name": "fuse_low", "label": "Low fuse", "kind": "text", "default": "62",
             "description": "Low fuse byte in hex (programmed bits are 0). 62, the factory value: internal 8 MHz RC "
                            "oscillator divided by 8 (CKDIV8), 1 MHz, 65 ms start-up. E2: 8 MHz. FF with a 16 MHz "
                            "Crystal between XTAL1 and XTAL2, as on the Arduino UNO"},
            {"name": "fuse_high", "label": "High fuse", "kind": "text", "default": "D9",
             "description": "High fuse byte in hex (programmed bits are 0). D9, the factory value; DE on the Arduino "
                            "UNO. Bit 7 RSTDISBL (PC6 as I/O), 6 DWEN, 5 SPIEN, 4 WDTON, 3 EESAVE, 2-1 BOOTSZ, "
                            "0 BOOTRST"},
            {"name": "fuse_extended", "label": "Extended fuse", "kind": "text", "default": "FF",
             "description": "Extended fuse byte in hex: BODLEVEL2:0 in bits 2-0. FF: brown-out detection off (the "
                            "factory value), FE 1.8 V, FD 2.7 V (the Arduino UNO), FC 4.3 V"},
        ]
        eeprom = "1 KB"
    else:
        fuses = [
            {"name": "fuse_low", "label": "Low fuse", "kind": "text", "default": "E1",
             "description": "Low fuse byte in hex (programmed bits are 0). E1, the factory value: internal RC "
                            "oscillator at 1 MHz, 65 ms start-up. EF with a 16 MHz Crystal between XTAL1 and XTAL2 "
                            "(and the high fuse C9)"},
            {"name": "fuse_high", "label": "High fuse", "kind": "text", "default": "D9",
             "description": "High fuse byte in hex (programmed bits are 0). D9, the factory value. Bit 7 RSTDISBL "
                            "(PC6 as I/O), 6 WDTON, 4 CKOPT, 3 EESAVE, 2-1 BOOTSZ, 0 BOOTRST"},
        ]
        eeprom = "512 bytes"
    return {
        "type": chip_id + "_pdip28",
        "name": title + "-PU (PDIP-28)",
        "category": "Microcontrollers",
        "prefix": "U",
        "description": description,
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-2, -17],
        "value_offset": [-2, 17],
        "parameters": [
            {"name": "firmware", "label": "Firmware", "kind": "file", "filter": "*.hex *.elf", "default": "",
             "description": "Firmware for the chip: an Intel HEX file (Microchip Studio, MPLAB X, avr-objcopy, the "
                            "Arduino IDE's exported binary) or an avr-gcc ELF file, whose fuses and EEPROM data apply "
                            "too; stored relative to the project"},
            *fuses,
            {"name": "lock", "label": "Lock bits", "kind": "text", "default": "FF",
             "description": "Lock byte in hex; FF: no lock. The boot lock bits restrict SPM and LPM"},
            {"name": "eeprom", "label": "EEPROM", "kind": "file", "filter": "*.bin", "default": "",
             "description": f"EEPROM contents ({eeprom}). SimNodus keeps what the firmware writes as "
                            "<designator>-eeprom.bin beside the project; empty: erased, or the firmware's own "
                            "EEPROM data"},
            {"name": "usart_link", "label": "USART to the computer", "kind": "text", "host_link": True,
             "default": "",
             "description": "The USART (PD1 TX, PD0 RX) linked to the computer as through a USB-serial adapter: a "
                            "serial port (one end of a virtual port pair such as com0com), tcp:4000 for programs "
                            "that speak RFC 2217, or empty for none"},
        ],
        "mcu": {
            "chip": chip_id,
            "firmware_parameter": "firmware",
            "ground_pin": "GND",
            # ATmega8A datasheet, DC characteristics at 5 V: VOL 0.7 V and VOH
            # 4.2 V at 20 mA (about 35 Ohm), pull-ups of 20-50 kOhm, VIL 0.2 VCC,
            # VIH 0.6 VCC.
            "pin_model": {"supply": 5.0, "output_resistance": 35, "pull_resistance": 35000,
                          "input_high": 3.0, "input_low": 1.5 if m328 else 1.0},
            "uarts": [{"name": "usart", "label": "USART (PD1 TX, PD0 RX)", "tx_pin": "PD1", "rx_pin": "PD0",
                       "link_parameter": "usart_link", "reset_circuit": ""}],
        },
        "spice": spice,
    }


# PDIP-40 pinout of the ATmega32/32A.
PINOUT40 = [
    (1, "PB0", "PB0/T0"), (2, "PB1", "PB1/T1"), (3, "PB2", "PB2/INT2/AIN0"), (4, "PB3", "PB3/OC0/AIN1"),
    (5, "PB4", "PB4/SS"), (6, "PB5", "PB5/MOSI"), (7, "PB6", "PB6/MISO"), (8, "PB7", "PB7/SCK"), (9, "RESET", "RESET"),
    (10, "VCC", "VCC"), (11, "GND", "GND"), (12, "XTAL2", "XTAL2"), (13, "XTAL1", "XTAL1"), (14, "PD0", "PD0/RXD"),
    (15, "PD1", "PD1/TXD"), (16, "PD2", "PD2/INT0"), (17, "PD3", "PD3/INT1"), (18, "PD4", "PD4/OC1B"),
    (19, "PD5", "PD5/OC1A"), (20, "PD6", "PD6/ICP1"), (21, "PD7", "PD7/OC2"), (22, "PC0", "PC0/SCL"),
    (23, "PC1", "PC1/SDA"), (24, "PC2", "PC2/TCK"), (25, "PC3", "PC3/TMS"), (26, "PC4", "PC4/TDO"),
    (27, "PC5", "PC5/TDI"), (28, "PC6", "PC6/TOSC1"), (29, "PC7", "PC7/TOSC2"), (30, "AVCC", "AVCC"),
    (31, "GND2", "GND"), (32, "AREF", "AREF"), (33, "PA7", "PA7/ADC7"), (34, "PA6", "PA6/ADC6"),
    (35, "PA5", "PA5/ADC5"), (36, "PA4", "PA4/ADC4"), (37, "PA3", "PA3/ADC3"), (38, "PA2", "PA2/ADC2"),
    (39, "PA1", "PA1/ADC1"), (40, "PA0", "PA0/ADC0"),
]


def chip40(chip_id, title, description):
    graphics = [{"rect": [-8, -21, 16, 42]}, {"arc": [0, -21, 0.8, 180, 180]},
                {"text": title, "at": [0, 0], "size": 0.9}]
    pins = []
    for number, name, label in PINOUT40:
        left = number <= 20
        y = -19 + 2 * ((number - 1) if left else (40 - number))
        x_end, x_body = (-10, -8) if left else (10, 8)
        graphics.append({"line": [x_end, y, x_body, y]})
        graphics.append({"text": label, "at": [-4.6 if left else 4.6, y], "size": 0.55})
        graphics.append({"text": str(number), "at": [-9.0 if left else 9.0, y - 0.5], "size": 0.45})
        pins.append({"name": name, "at": [x_end, y], "dir": "left" if left else "right"})
    gpio = [name for _, name, _ in PINOUT40 if name.startswith("P") or name == "RESET"]
    spice = ["R_{ref}_gnd2 {node:GND} {node:GND2} 1m",
             "R_{ref}_vcc {node:VCC} {node:GND} 2.5k",
             "R_{ref}_avcc {node:AVCC} {node:GND} 100k",
             "R_{ref}_aref {node:AREF} {node:GND} 32k",
             "R_{ref}_xtal1 {node:XTAL1} {node:GND} 100Meg",
             "R_{ref}_xtal2 {node:XTAL2} {node:GND} 100Meg"]
    for name in gpio:
        low = name.lower()
        spice.append(f"R_{{ref}}_{low} {{node:{name}}} {{node:GND}} 100Meg")
        spice.append(f"B_{{ref}}_{low} {{node:GND}} {{node:{name}}} "
                     f"I=V({{ext:{low}_i}})-V({{ext:{low}_g}})*(V({{node:{name}}})-V({{node:GND}}))")
    return {
        "type": chip_id + "_pdip40",
        "name": title + "-PU (PDIP-40)",
        "category": "Microcontrollers",
        "prefix": "U",
        "description": description,
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-2, -23],
        "value_offset": [-2, 23],
        "parameters": [
            {"name": "firmware", "label": "Firmware", "kind": "file", "filter": "*.hex *.elf", "default": "",
             "description": "Firmware for the chip: an Intel HEX file (Microchip Studio, MPLAB X, avr-objcopy) or an "
                            "avr-gcc ELF file, whose fuses and EEPROM data apply too; stored relative to the project"},
            {"name": "fuse_low", "label": "Low fuse", "kind": "text", "default": "E1",
             "description": "Low fuse byte in hex (programmed bits are 0). E1, the factory value: internal RC "
                            "oscillator at 1 MHz, 65 ms start-up. EF with a 16 MHz Crystal between XTAL1 and XTAL2 "
                            "(and the high fuse 89)"},
            {"name": "fuse_high", "label": "High fuse", "kind": "text", "default": "99",
             "description": "High fuse byte in hex (programmed bits are 0). 99, the factory value: JTAGEN programmed, "
                            "so PC2-PC5 are the JTAG pins until the firmware sets JTD; D9 frees them. Bit 7 OCDEN, "
                            "6 JTAGEN, 5 SPIEN, 4 CKOPT, 3 EESAVE, 2-1 BOOTSZ, 0 BOOTRST"},
            {"name": "lock", "label": "Lock bits", "kind": "text", "default": "FF",
             "description": "Lock byte in hex; FF: no lock. The boot lock bits restrict SPM and LPM"},
            {"name": "eeprom", "label": "EEPROM", "kind": "file", "filter": "*.bin", "default": "",
             "description": "EEPROM contents (1 KB). SimNodus keeps what the firmware writes as "
                            "<designator>-eeprom.bin beside the project; empty: erased, or the firmware's own "
                            "EEPROM data"},
            {"name": "usart_link", "label": "USART to the computer", "kind": "text", "host_link": True,
             "default": "",
             "description": "The USART (PD1 TX, PD0 RX) linked to the computer as through a USB-serial adapter: a "
                            "serial port (one end of a virtual port pair such as com0com), tcp:4000 for programs "
                            "that speak RFC 2217, or empty for none"},
        ],
        "mcu": {
            "chip": chip_id,
            "firmware_parameter": "firmware",
            "ground_pin": "GND",
            # ATmega32A datasheet, DC characteristics at 5 V (as the ATmega8A's).
            "pin_model": {"supply": 5.0, "output_resistance": 35, "pull_resistance": 35000,
                          "input_high": 3.0, "input_low": 1.0},
            "uarts": [{"name": "usart", "label": "USART (PD1 TX, PD0 RX)", "tx_pin": "PD1", "rx_pin": "PD0",
                       "link_parameter": "usart_link", "reset_circuit": ""}],
        },
        "spice": spice,
    }


def uno():
    left = ["IOREF", "RESET", "3V3", "5V", "GND1", "GND2", "VIN", None, "A0", "A1", "A2", "A3", "A4", "A5"]
    right = ["AREF", "GND3", "D13", "D12", "D11", "D10", "D9", "D8", None, "D7", "D6", "D5", "D4", "D3", "D2",
             "D1", "D0"]
    labels = {"GND1": "GND", "GND2": "GND", "GND3": "GND", "D13": "D13/SCK", "D12": "D12/MISO", "D11": "~D11/MOSI",
              "D10": "~D10/SS", "D9": "~D9", "D6": "~D6", "D5": "~D5", "D3": "~D3", "D1": "D1/TX", "D0": "D0/RX",
              "A4": "A4/SDA", "A5": "A5/SCL"}
    graphics = [{"rect": [-10, -19, 20, 38]},
                {"rect": [-7, -21, 6, 4]}, {"text": "USB", "at": [-4, -19.6], "size": 0.7},
                {"rect": [-3, 2, 6, 10]}, {"text": "ATmega328P", "at": [0, 7], "size": 0.55},
                {"text": "Arduino", "at": [0, -6], "size": 1.1}, {"text": "UNO", "at": [0, -4.3], "size": 1.1},
                {"rect": [0, -17.5, 3.5, 1.6]}, {"text": "RESET", "at": [1.75, -15], "size": 0.55},
                # The L LED on D13: a faint circle while it is off (the run draws it lit).
                {"arc": [1, -12, 0.6, 0, 360]}, {"text": "L", "at": [2.6, -12], "size": 0.6}]
    pins = []
    gpio = []
    for column, names in ((-1, left), (1, right)):
        for i, name in enumerate(names):
            if name is None:
                continue
            y = -16 + 2 * i
            x_end, x_body = (-12, -10) if column < 0 else (12, 10)
            graphics.append({"line": [x_end, y, x_body, y]})
            graphics.append({"text": labels.get(name, name), "at": [-7.6 if column < 0 else 7.4, y], "size": 0.55})
            pins.append({"name": name, "at": [x_end, y], "dir": "left" if column < 0 else "right"})
            if name[0] in "DA" and name[1:].isdigit() or name == "RESET":
                gpio.append(name)
    spice = ["R_{ref}_g12 {node:GND1} {node:GND2} 1m", "R_{ref}_g13 {node:GND1} {node:GND3} 1m",
             # The USB supply on 5V and the board's 3.3 V regulator.
             "V_{ref}_5v usb_{ref} {node:GND1} DC 5", "R_{ref}_5v usb_{ref} {node:5V} 0.1",
             "V_{ref}_3v3 reg_{ref} {node:GND1} DC 3.3", "R_{ref}_3v3 reg_{ref} {node:3V3} 0.5",
             "R_{ref}_ioref {node:IOREF} {node:5V} 1m", "R_{ref}_vin {node:VIN} {node:GND1} 10Meg",
             "R_{ref}_aref {node:AREF} {node:GND1} 32k",
             "R_{ref}_reset_pullup {node:RESET} {node:5V} 10k"]
    for name in gpio:
        low = name.lower()
        spice.append(f"R_{{ref}}_{low} {{node:{name}}} {{node:GND1}} 100Meg")
        spice.append(f"B_{{ref}}_{low} {{node:GND1}} {{node:{name}}} "
                     f"I=V({{ext:{low}_i}})-V({{ext:{low}_g}})*(V({{node:{name}}})-V({{node:GND1}}))")
    return {
        "type": "arduino_uno",
        "name": "Arduino UNO R3",
        "category": "Microcontrollers",
        "prefix": "U",
        "description": "Arduino UNO R3 (experimental): an ATmega328P at 16 MHz with Optiboot, as the Arduino IDE "
                       "uploads to it. Runs a sketch's .hex or .elf (Sketch > Export Compiled Binary), or waits for "
                       "an upload through its USB connector; powered by the USB (5V and 3V3 are outputs)",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-2, -23],
        "value_offset": [-2, 21],
        "parameters": [
            {"name": "firmware", "label": "Firmware", "kind": "file", "filter": "*.hex *.elf *.bin", "default": "",
             "description": "The sketch: an Intel HEX file (the Arduino IDE's Sketch > Export Compiled Binary, with "
                            "or without the bootloader) or an avr-gcc ELF file; stored relative to the project. Empty: "
                            "the board has only its bootloader and waits for an upload through the USB connector. "
                            "After an upload, SimNodus keeps the flash as <designator>-flash.bin"},
            {"name": "eeprom", "label": "EEPROM", "kind": "file", "filter": "*.bin", "default": "",
             "description": "EEPROM contents (1 KB). SimNodus keeps what the sketch writes as "
                            "<designator>-eeprom.bin beside the project; empty: erased"},
            {"name": "usb_link", "label": "USB to the computer", "kind": "text", "host_link": True, "default": "",
             "description": "The board's USB connector linked to the computer: a serial port (one end of a virtual "
                            "port pair such as com0com; the Arduino IDE uploads and opens its Serial Monitor on the "
                            "other end), tcp:4000 for programs that speak RFC 2217, or empty for none. DTR resets "
                            "the board, as the IDE does before an upload"},
        ],
        "mcu": {
            "chip": "atmega328p",
            "firmware_parameter": "firmware",
            "ground_pin": "GND1",
            "pin_model": {"supply": 5.0, "output_resistance": 35, "pull_resistance": 35000,
                          "input_high": 3.0, "input_low": 1.5},
            "led": {"pin": "D13", "at": [1, -12], "radius": 0.6},
            "reset_button": [0, -17.5, 3.5, 1.6],
            "uarts": [{"name": "usart", "label": "USB (USART: D1 TX, D0 RX)", "tx_pin": "D1", "rx_pin": "D0",
                       "link_parameter": "usb_link", "reset_circuit": "dtr_reset"}],
            # The board's fuses as Arduino burns them (boards.txt), its 16 MHz
            # crystal and Optiboot (third_party/optiboot).
            "parameters": {"fuse_low": "FF", "fuse_high": "DE", "fuse_extended": "FD", "lock": "0F",
                           "crystal_hz": "16000000", "bootloader": "optiboot_atmega328.hex"},
        },
        "spice": spice,
    }


def crystal():
    return {
        "type": "crystal",
        "name": "Crystal",
        "category": "Passive",
        "prefix": "Y",
        "description": "Quartz crystal or ceramic resonator. Between a microcontroller's oscillator pins (XTAL1 "
                       "and XTAL2 of an AVR, PH0 and PH1 of an STM32F407) its frequency is the chip's clock when "
                       "the chip selects the crystal; the oscillation itself is not simulated (add the usual 22 pF "
                       "capacitors to ground as on the board)",
        "symbol": {"graphics": [{"line": [-3, 0, -1.2, 0]}, {"line": [1.2, 0, 3, 0]},
                                {"line": [-1.2, -1.2, -1.2, 1.2]}, {"line": [1.2, -1.2, 1.2, 1.2]},
                                {"rect": [-0.7, -1.6, 1.4, 3.2]}],
                   "pins": [{"name": "1", "at": [-3, 0], "dir": "left"}, {"name": "2", "at": [3, 0], "dir": "right"}]},
        "value_parameter": "frequency",
        "parameters": [{"name": "frequency", "label": "Frequency", "unit": "Hz", "default": "16M",
                        "min": "32.768k", "max": "50M",
                        "description": "Frequency of the crystal or resonator"}],
        "spice": ["R_{ref} {node:1} {node:2} 1G"],  # DC: open
    }


def write(path, data):
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        json.dump(data, out, indent=2, ensure_ascii=False)
        out.write("\n")


def main():
    mcu = os.path.join(ROOT, "resources", "library", "mcu")
    write(os.path.join(mcu, "atmega8_pdip28.json"),
          chip("atmega8", "ATmega8", "Atmel ATmega8-16PU in PDIP-28: 8 KB flash, 1 KB SRAM, "
               "512 bytes of EEPROM, up to 16 MHz at 4.5-5.5 V. Runs .hex or .elf firmware with its fuses; wire VCC, "
               "AVCC and GND (5 V)"))
    write(os.path.join(mcu, "atmega8a_pdip28.json"),
          chip("atmega8a", "ATmega8A", "Microchip ATmega8A-PU in PDIP-28: 8 KB flash, 1 KB "
               "SRAM, 512 bytes of EEPROM, up to 16 MHz at 2.7-5.5 V. Runs .hex or .elf firmware with its fuses; wire "
               "VCC, AVCC and GND (the pins are simulated at 5 V)"))
    write(os.path.join(mcu, "atmega328p_pdip28.json"),
          chip("atmega328p", "ATmega328P", "Microchip ATmega328P-PU in PDIP-28 (experimental): 32 KB flash, 2 KB "
               "SRAM, 1 KB of EEPROM, up to 20 MHz at 1.8-5.5 V; the Arduino UNO's chip. Runs .hex or .elf firmware "
               "with its fuses; wire VCC, AVCC and GND (the pins are simulated at 5 V)", m328=True))
    write(os.path.join(mcu, "atmega328_pdip28.json"),
          chip("atmega328", "ATmega328", "Microchip ATmega328-PU in PDIP-28 (experimental): the ATmega328P without "
               "picoPower, with its own signature (1E 95 14); 32 KB flash, 2 KB SRAM, 1 KB of EEPROM, up to 20 MHz. "
               "Runs .hex or .elf firmware with its fuses; wire VCC, AVCC and GND (the pins are simulated at 5 V)",
               m328=True))
    write(os.path.join(mcu, "atmega32_pdip40.json"),
          chip40("atmega32", "ATmega32", "Atmel ATmega32-16PU in PDIP-40 (experimental): 32 KB flash, 2 KB SRAM, "
                 "1 KB of EEPROM, ports A to D, up to 16 MHz at 4.5-5.5 V. Runs .hex or .elf firmware with its fuses; "
                 "PC2-PC5 are JTAG pins at the factory fuses; wire VCC, AVCC and GND (5 V)"))
    write(os.path.join(mcu, "atmega32a_pdip40.json"),
          chip40("atmega32a", "ATmega32A", "Microchip ATmega32A-PU in PDIP-40 (experimental): 32 KB flash, 2 KB "
                 "SRAM, 1 KB of EEPROM, ports A to D, up to 16 MHz at 2.7-5.5 V. Runs .hex or .elf firmware with its "
                 "fuses; PC2-PC5 are JTAG pins at the factory fuses; wire VCC, AVCC and GND (the pins are simulated "
                 "at 5 V)"))
    write(os.path.join(mcu, "arduino_uno.json"), uno())
    write(os.path.join(ROOT, "resources", "library", "passive", "crystal.json"), crystal())


if __name__ == "__main__":
    main()
