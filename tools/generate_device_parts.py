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


def part(type_, name, description, symbol, upper, kind, spice, output_resistance=50):
    return {
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
    for p in [hc595(), max7219()]:
        p["description"] = short.get(p["type"], p["description"])
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
