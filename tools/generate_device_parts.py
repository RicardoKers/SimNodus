"""Writes the digital devices of resources/library/devices (SN-203, ADR 0102).

These chips work together with a microcontroller board: their logic runs in
the board's runner, in step with the firmware (src/devices), and the nets
they share with the board are resolved there at once. The SPICE template only
gives each output pin a driver controlled by the runner, like a board pin
({ext:<pin>_g} conductance and {ext:<pin>_i} current, against the GND pin),
and every signal pin a 100 MOhm path to GND so no node floats. The supply pin
must be wired: the outputs drive to its voltage, and without it the chip does
nothing.

- 74HC595: 8-bit shift register with output latches (NXP pin names: DS, SHCP,
  STCP, MR, OE, Q0-Q7, Q7S).
- 24C02: 256-byte I2C EEPROM (SCL, SDA, A0-A2, WP).
- LM75: I2C temperature sensor (SCL, SDA, A0-A2, OS); the temperature is a
  parameter.
- PCF8574: I2C 8-bit quasi-bidirectional I/O expander (SCL, SDA, A0-A2, INT,
  P0-P7); its pins read the circuit (buttons), so they work as inputs too.
- MAX7219: 8-digit LED display driver (DIN, CLK, LOAD, DOUT, DIG0-DIG7, SEGA-SEGG,
  SEGDP); its segment outputs source about 40 mA at 5 V like the chip with its
  current-setting resistor, so the LEDs need no series resistors.

Run from the repository root: python tools/generate_device_parts.py
"""
import json
import os

FOLDER = "resources/library/devices"
CATEGORY = "Chips for microcontrollers"


def ic_symbol(title, left, right, top, bottom, inverted=(), optional=()):
    rows = max(len(left), len(right))
    height = 2 * rows + 2
    upper = -(height // 2)
    graphics = [{"rect": [-4, upper, 8, height]}, {"text": title, "at": [0, upper + 1.2], "size": 0.8}]
    pins = []
    for names, x_tip, x_edge, direction in ((left, -6, -4, "left"), (right, 6, 4, "right")):
        first = -(len(names) - 1)
        for i, name in enumerate(names):
            y = first + 2 * i
            graphics.append({"line": [x_tip, y, x_edge, y]})
            graphics.append({"text": name, "at": [x_edge + (1.3 if x_edge < 0 else -1.2), y], "size": 0.6})
            if name in inverted:  # active low: a bubble at the body
                cx = x_edge - 0.3 if x_edge < 0 else x_edge + 0.3
                graphics.append({"circle": [cx, y, 0.3]})
            pin = {"name": name, "at": [x_tip, y], "dir": direction}
            if name in optional:
                pin["optional"] = True
            pins.append(pin)
    graphics += [{"line": [0, upper - 2, 0, upper]}, {"text": top, "at": [0, upper + 0.5], "size": 0.55},
                 {"line": [0, -upper, 0, -upper + 2]}, {"text": bottom, "at": [0, -upper - 0.5], "size": 0.55}]
    pins += [{"name": top, "at": [0, upper - 2], "dir": "up"}, {"name": bottom, "at": [0, -upper + 2], "dir": "down"}]
    return {"graphics": graphics, "pins": pins}, upper


def device_spice(signals, outputs, supply="VCC", ground="GND"):
    spice = [f"R_{{ref}}_h{p.lower()} {{node:{p}}} {{node:{ground}}} 100Meg" for p in signals]
    for p in outputs:
        n = p.lower()
        spice.append(f"B_{{ref}}_{n} {{node:{ground}}} {{node:{p}}} "
                     f"I=V({{ext:{n}_i}})-V({{ext:{n}_g}})*(V({{node:{p}}})-V({{node:{ground}}}))")
    spice.append(f"R_{{ref}}_q {{node:{supply}}} {{node:{ground}}} 100k")  # quiescent current
    return spice


def part(type_, name, description, symbol, upper, kind, spice, output_resistance=50, parameters=None):
    result = {
        "type": type_,
        "name": name,
        "category": CATEGORY,
        "prefix": "U",
        "description": description,
        "symbol": symbol,
        "designator_offset": [5, upper - 1],
        "value_offset": [5, -upper + 1],
        "device": {"kind": kind, "supply_pin": "VCC", "ground_pin": "GND", "output_resistance": output_resistance},
        "spice": spice,
    }
    if parameters:
        result["parameters"] = parameters
        result["value_parameter"] = parameters[0]["name"]
    return result


def hc595():
    inputs = ["DS", "SHCP", "STCP", "MR", "OE"]
    outputs = [f"Q{i}" for i in range(8)] + ["Q7S"]
    symbol, upper = ic_symbol("74HC595", inputs, outputs, "VCC", "GND", inverted=("MR", "OE"),
                              optional=tuple(outputs))
    return part("74hc595", "Shift register 74HC595",
                "8-bit serial-in shift register with output latches (74HC595, NXP pin names): DS shifts in at each "
                "rising edge of SHCP (Q0 first), a rising edge of STCP copies the shift register to Q0-Q7, MR low "
                "clears the shift register, OE high turns Q0-Q7 off (high impedance); Q7S, the last stage, feeds the "
                "DS of a second 74HC595. Tie MR to VCC and OE to GND for normal use; supply 2-6 V on VCC and GND. "
                "Works together with a microcontroller board.",
                symbol, upper, "74hc595", device_spice(inputs + outputs, outputs))


I2C_NOTE = ("SDA is open drain: SCL and SDA need pull-up resistors (4.7 kOhm to the supply), as on a real board; "
            "A0-A2 set the low address bits (tie them to GND or VCC).")


def eeprom_24c02():
    inputs = ["SCL", "A0", "A1", "A2", "WP"]
    symbol, upper = ic_symbol("24C02", ["SCL", "SDA", "A0", "A1", "A2"], ["WP"], "VCC", "GND")
    return part("24c02", "EEPROM 24C02 (I2C)",
                "2 Kbit (256-byte) serial EEPROM like the AT24C02 at I2C address 0x50 + A2 A1 A0: write a word address "
                "and up to 8 bytes of one page, stored at STOP in 5 ms (the chip does not acknowledge its address "
                "meanwhile); reads start at the word address and continue through the memory; WP high protects it. "
                "Starts erased (0xFF). " + I2C_NOTE + " Supply 1.8-5.5 V. Works together with a microcontroller board.",
                symbol, upper, "24c02", device_spice(inputs + ["SDA"], ["SDA"]))


def lm75():
    inputs = ["SCL", "A0", "A1", "A2"]
    symbol, upper = ic_symbol("LM75", ["SCL", "SDA", "A0", "A1", "A2"], ["OS"], "VCC", "GND", optional=("OS",))
    return part("lm75", "Temperature sensor LM75 (I2C)",
                "Digital temperature sensor (LM75) at I2C address 0x48 + A2 A1 A0: register 0 reads the temperature as "
                "two bytes (whole degrees, then bit 7 for half a degree), 1 is the configuration, 2 and 3 are THYST "
                "(75 C) and TOS (80 C); the open-drain OS output goes low above TOS and is released again below THYST. "
                + I2C_NOTE + " Supply 2.8-5.5 V. Works together with a microcontroller board.",
                symbol, upper, "lm75", device_spice(inputs + ["SDA", "OS"], ["SDA", "OS"]),
                parameters=[{"name": "temperature", "label": "Temperature", "unit": "°C", "default": "25",
                             "min": "-55", "max": "125",
                             "description": "Temperature the sensor measures during the run"}])


def pcf8574():
    ports = [f"P{i}" for i in range(8)]
    inputs = ["SCL", "A0", "A1", "A2"]
    symbol, upper = ic_symbol("PCF8574", ["SCL", "SDA", "A0", "A1", "A2", "INT"], ports, "VCC", "GND",
                              inverted=("INT",), optional=tuple(ports) + ("INT",))
    return part("pcf8574", "I/O expander PCF8574 (I2C)",
                "8-bit quasi-bidirectional I/O expander (PCF8574) at I2C address 0x20 + A2 A1 A0 (PCF8574A: 0x38): a "
                "written byte sets P0-P7, a 0 pulls the pin low (an LED from VCC lights), a 1 leaves it high through "
                "a weak pull-up so it can be read as an input (a push button to GND); a read returns the pin levels; "
                "INT (open drain) goes low when an input changes. Drives an HD44780 LCD as on the common I2C LCD "
                "backpacks. " + I2C_NOTE + " Supply 2.5-6 V. Works together with a microcontroller board.",
                symbol, upper, "pcf8574", device_spice(inputs + ["SDA", "INT"] + ports, ["SDA", "INT"] + ports),
                parameters=[{"name": "variant", "label": "Variant", "unit": "", "default": "PCF8574",
                             "choices": ["PCF8574", "PCF8574A"],
                             "description": "PCF8574 answers at 0x20-0x27, PCF8574A at 0x38-0x3F"}])


def max7219():
    inputs = ["DIN", "CLK", "LOAD"]
    digits = [f"DIG{i}" for i in range(8)]
    segments = [f"SEG{s}" for s in "ABCDEFG"] + ["SEGDP"]
    outputs = ["DOUT"] + digits + segments
    symbol, upper = ic_symbol("MAX7219", inputs + ["DOUT"], digits + segments, "VCC", "GND",
                              optional=tuple(outputs))
    return part("max7219", "LED display driver MAX7219",
                "Serially interfaced 8-digit LED display driver (MAX7219): 16-bit words shift in on DIN at rising CLK "
                "(MSB first: 4 address bits, 8 data bits) and a rising LOAD stores them (registers 1-8 digits, 9 "
                "decode mode, 10 intensity, 11 scan limit, 12 shutdown, 15 display test). It scans the digits about "
                "800 times a second: the active DIG pin sinks the digit's current, SEG A-G and DP source the segment "
                "current (about 40 mA at 5 V, no series resistors needed). Drives eight common-cathode digits or an "
                "8x8 LED matrix (SEG to the anodes, DIG to the cathodes); DOUT feeds the DIN of a second MAX7219. "
                "Starts in shutdown: write 1 to register 12. Supply 4-5.5 V on VCC and GND. Works together with a "
                "microcontroller board.",
                symbol, upper, "max7219", device_spice(inputs + outputs, outputs), output_resistance=75)


def main():
    os.makedirs(FOLDER, exist_ok=True)
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "part_descriptions.json"), encoding="utf-8") as fh:
        short = json.load(fh)
    for p in [hc595(), max7219(), eeprom_24c02(), lm75(), pcf8574()]:
        p["description"] = short.get(p["type"], p["description"])
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
