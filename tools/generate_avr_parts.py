"""Generate the AVR chip parts (experiment E-10, SN-258) and the Crystal part.

resources/library/mcu/atmega8_pdip28.json and atmega8a_pdip28.json: the bare
chips in PDIP-28, pins named as on the chip (PB0..PD7; PC6 is RESET until
RSTDISBL is programmed, PB6/PB7 are XTAL1/XTAL2 when the fuses select a
crystal), VCC, GND, AVCC and AREF. Each GPIO drives the circuit through the
runner's pin model (5 V; the supply voltage on VCC is not read yet: SN-259).
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


def chip(chip_id, title, description):
    graphics = [{"rect": [-7, -15, 14, 30]}, {"arc": [0, -15, 0.8, 180, 180]},
                {"text": title, "at": [0, 0], "size": 0.9}]
    pins = []
    for number, name, label in PINOUT:
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
            {"name": "fuse_low", "label": "Low fuse", "kind": "text", "default": "E1",
             "description": "Low fuse byte in hex (programmed bits are 0). E1, the factory value: internal RC "
                            "oscillator at 1 MHz, 65 ms start-up. EF with a 16 MHz Crystal between XTAL1 and XTAL2 "
                            "(and the high fuse C9)"},
            {"name": "fuse_high", "label": "High fuse", "kind": "text", "default": "D9",
             "description": "High fuse byte in hex (programmed bits are 0). D9, the factory value. Bit 7 RSTDISBL "
                            "(PC6 as I/O), 6 WDTON, 4 CKOPT, 3 EESAVE, 2-1 BOOTSZ, 0 BOOTRST"},
            {"name": "lock", "label": "Lock bits", "kind": "text", "default": "FF",
             "description": "Lock byte in hex; FF: no lock. The boot lock bits restrict SPM and LPM"},
            {"name": "eeprom", "label": "EEPROM", "kind": "file", "filter": "*.bin", "default": "",
             "description": "EEPROM contents (512 bytes). SimNodus keeps what the firmware writes as "
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
                          "input_high": 3.0, "input_low": 1.0},
            "uarts": [{"name": "usart", "label": "USART (PD1 TX, PD0 RX)", "tx_pin": "PD1", "rx_pin": "PD0",
                       "link_parameter": "usart_link", "reset_circuit": ""}],
        },
        "spice": spice,
    }


def crystal():
    return {
        "type": "crystal",
        "name": "Crystal",
        "category": "Passive",
        "prefix": "Y",
        "description": "Quartz crystal or ceramic resonator. Between a microcontroller's XTAL1 and XTAL2 its "
                       "frequency is the chip's clock when the fuses select an external crystal; the oscillation "
                       "itself is not simulated (add the usual 22 pF capacitors to ground as on the board)",
        "symbol": {"graphics": [{"line": [-3, 0, -1.2, 0]}, {"line": [1.2, 0, 3, 0]},
                                {"line": [-1.2, -1.2, -1.2, 1.2]}, {"line": [1.2, -1.2, 1.2, 1.2]},
                                {"rect": [-0.7, -1.6, 1.4, 3.2]}],
                   "pins": [{"name": "1", "at": [-3, 0], "dir": "left"}, {"name": "2", "at": [3, 0], "dir": "right"}]},
        "value_parameter": "frequency",
        "parameters": [{"name": "frequency", "label": "Frequency", "unit": "Hz", "default": "16M",
                        "min": "32.768k", "max": "20M",
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
          chip("atmega8", "ATmega8", "Atmel ATmega8-16PU in PDIP-28 (experimental, E-10): 8 KB flash, 1 KB SRAM, "
               "512 bytes of EEPROM, up to 16 MHz at 4.5-5.5 V. Runs .hex or .elf firmware with its fuses; wire VCC, "
               "AVCC and GND (5 V)"))
    write(os.path.join(mcu, "atmega8a_pdip28.json"),
          chip("atmega8a", "ATmega8A", "Microchip ATmega8A-PU in PDIP-28 (experimental, E-10): 8 KB flash, 1 KB "
               "SRAM, 512 bytes of EEPROM, up to 16 MHz at 2.7-5.5 V. Runs .hex or .elf firmware with its fuses; wire "
               "VCC, AVCC and GND (the pins are simulated at 5 V)"))
    write(os.path.join(ROOT, "resources", "library", "passive", "crystal.json"), crystal())


if __name__ == "__main__":
    main()
