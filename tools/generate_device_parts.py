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
- HD44780 character LCD (SN-206): the module's 16 pins on top as on a real
  module (VSS VDD V0 RS RW E D0-D7 A K), rows and columns as parameters; the
  editor draws its text in the screen area; A and K are the backlight LED.
- ROM and RAM (SN-212): generic parallel memories of 16 to 64K bytes (A0-A15,
  D0-D7, CE, OE and the RAM's WE, active low); the size is a parameter and the
  address pins beyond it are ignored; contents from a .bin or Intel .hex file.
  Category *Memories*.
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


def part(type_, name, description, symbol, upper, kind, spice, output_resistance=50, parameters=None, driven=(),
         category=CATEGORY):
    result = {
        "type": type_,
        "name": name,
        "category": category,
        "prefix": "U",
        "description": description,
        "symbol": symbol,
        "designator_offset": [5, upper - 1],
        "value_offset": [5, -upper + 1],
        "device": {"kind": kind, "supply_pin": "VCC", "ground_pin": "GND", "output_resistance": output_resistance},
        "spice": spice,
    }
    if driven:
        result["device"]["driven_pins"] = list(driven)  # push-pull outputs that always drive
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
                symbol, upper, "74hc595", device_spice(inputs + outputs, outputs), driven=["Q7S"])


I2C_NOTE = ("SDA is open drain: SCL and SDA need pull-up resistors (4.7 kOhm to the supply), as on a real board; "
            "A0-A2 set the low address bits (tie them to GND or VCC).")


def eeprom_24c02():
    inputs = ["SCL", "A0", "A1", "A2", "WP"]
    symbol, upper = ic_symbol("24C02", ["SCL", "SDA", "A0", "A1", "A2"], ["WP"], "VCC", "GND")
    p = part("24c02", "EEPROM 24C02 (I2C)",
             "2 Kbit (256-byte) serial EEPROM like the AT24C02 at I2C address 0x50 + A2 A1 A0: write a word address "
             "and up to 8 bytes of one page, stored at STOP in 5 ms (the chip does not acknowledge its address "
             "meanwhile); reads start at the word address and continue through the memory; WP high protects it. "
             "Starts erased (0xFF) or with its contents file. " + I2C_NOTE + " Supply 1.8-5.5 V. Works together with "
             "a microcontroller board.",
             symbol, upper, "24c02", device_spice(inputs + ["SDA"], ["SDA"]),
             parameters=[{"name": "contents", "label": "Contents file", "kind": "file", "filter": "*.bin *.hex",
                          "default": "", "description": "Contents the EEPROM starts each run with (.bin or Intel "
                                                        ".hex, stored relative to the project); empty: erased"}])
    p["device"].update({"memory_size": 256, "contents_parameter": "contents"})
    del p["value_parameter"]  # the file name would crowd the symbol
    return p


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
                             "min": "-55", "max": "125", "live": True,
                             "description": "Temperature the sensor measures; change it while running to test the "
                                            "circuit"}])


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


def lcd_hd44780():
    names = ["VSS", "VDD", "V0", "RS", "RW", "E"] + [f"D{i}" for i in range(8)] + ["A", "K"]
    graphics = [{"rect": [-17, -3, 34, 14]}]
    pins = []
    for i, name in enumerate(names):
        x = -15 + 2 * i
        pin = {"name": name, "at": [x, -5], "dir": "up"}
        if name in ("V0", "A", "K") or name in [f"D{k}" for k in range(4)]:
            pin["optional"] = True  # contrast, backlight, and D0-D3 in 4-bit mode
        pins.append(pin)
        graphics.append({"line": [x, -5, x, -3]})
        graphics.append({"text": name, "at": [x, -2.2], "size": 0.45})
    outputs = [f"D{i}" for i in range(8)]
    spice = device_spice(["V0", "RS", "RW", "E"] + outputs, outputs, supply="VDD", ground="VSS")
    # Backlight: a white LED with its series resistor between A and K.
    spice += ["R_{ref}_bl {node:A} bl_{ref} 47", "D_{ref}_bl bl_{ref} {node:K} LCDBL"]
    p = part("lcd_hd44780", "Character LCD (HD44780)",
             "Character LCD module with an HD44780 controller, 1, 2 or 4 rows of 8 to 40 characters (set in its "
             "properties): RS, RW and E with D0-D7 (8-bit) or D4-D7 (4-bit) from a microcontroller, or from a "
             "PCF8574 as on the common I2C backpacks; supply 5 V or 3.3 V on VDD and VSS; A and K are the backlight. "
             "The text appears on the schematic while running. Works together with a microcontroller board.",
             {"graphics": graphics, "pins": pins}, -3, "hd44780", spice,
             parameters=[{"name": "rows", "label": "Rows", "unit": "", "default": "2", "choices": ["1", "2", "4"],
                          "description": "Rows of characters"},
                         {"name": "columns", "label": "Columns", "unit": "", "default": "16",
                          "choices": ["8", "16", "20", "40"], "description": "Characters per row"}])
    p["category"] = "Displays"
    p["device"].update({"supply_pin": "VDD", "ground_pin": "VSS"})
    p["screen"] = {"rect": [-14, 0, 28, 8], "rows_parameter": "rows", "columns_parameter": "columns"}
    p["models"] = [".model LCDBL D(IS=6.7e-28 N=2 RS=8)"]
    p["designator_offset"] = [-17, -8]
    p["value_offset"] = [10, -8]
    del p["value_parameter"]  # rows and columns show on the screen itself
    return p


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
                symbol, upper, "max7219", device_spice(inputs + outputs, outputs), output_resistance=75,
                driven=outputs)


MEMORY_SIZES = ["16", "32", "64", "128", "256", "512", "1K", "2K", "4K", "8K", "16K", "32K", "64K"]


def memory(ram):
    address = [f"A{i}" for i in range(16)]
    data = [f"D{i}" for i in range(8)]
    control = ["CE", "OE"] + (["WE"] if ram else [])
    title = "RAM" if ram else "ROM"
    symbol, upper = ic_symbol(title, address, data + control, "VCC", "GND", inverted=tuple(control),
                              optional=tuple(address + data))
    what = ("Static RAM: with CE and OE low and WE high it drives D0-D7 with the addressed byte; with CE and WE low "
            "it takes D0-D7 and stores them when WE (or CE) rises. Starts with every byte 0, or with its contents "
            "file." if ram else
            "ROM: with CE and OE low it drives D0-D7 with the addressed byte, otherwise D0-D7 are off (high "
            "impedance). Its contents come from a .bin or Intel .hex file; without one every byte is 0xFF.")
    p = part(title.lower(), f"{title} (parallel)",
             f"Generic parallel {title} of 16 bytes to 64 KB (the size is a property): A0-A15 address a byte, the "
             "address pins beyond the size are ignored. " + what + " Ideal timing: no access or write time. Works "
             "with a microcontroller board's pins or on its own in a circuit; Window > Memory Viewer shows its "
             "contents. Supply on VCC and GND.",
             symbol, upper, title.lower(), device_spice(address + data + control, data),
             parameters=[{"name": "size", "label": "Size (bytes)", "choices": MEMORY_SIZES,
                          "default": "2K" if ram else "256", "description": "Number of 8-bit words; A0 up to the "
                                                                           "address pins the size needs are used"},
                         {"name": "contents", "label": "Contents file", "kind": "file", "filter": "*.bin *.hex",
                          "default": "", "description": "Contents the memory starts each run with (.bin or Intel "
                                                        ".hex, stored relative to the project)"}],
             category="Memories")
    p["device"].update({"memory_size_parameter": "size", "memory_fill": 0 if ram else 255,
                        "contents_parameter": "contents"})
    return p


def main():
    os.makedirs(FOLDER, exist_ok=True)
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "part_descriptions.json"), encoding="utf-8") as fh:
        short = json.load(fh)
    for p in [hc595(), max7219(), eeprom_24c02(), lm75(), pcf8574(), lcd_hd44780(), memory(False), memory(True)]:
        p["description"] = short.get(p["type"], p["description"])
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
