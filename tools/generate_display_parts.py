"""Writes the 7-segment displays and BCD decoders of resources/library/display
(SN-202).

Segment pins are named in lower case (a-g, dp), as on datasheets; the
decoder's BCD inputs are A-D. A display is eight LEDs (segments a-g and the decimal point) with a common
cathode or a common anode; each segment has a 0 V measuring source, so the
editor lights each drawn segment with its own current. The decoders are
behavioral like the logic gates (hidden supply VDD, inputs at VDD/2, outputs
through about 10 ns and 50 ohm): the 4511 drives a common-cathode display
(outputs high = segment on) and blanks the codes 10-15; the 7447 drives a
common-anode display (outputs low = segment on) with its own shapes for 10-15
reduced to blank here.

Run from the repository root: python tools/generate_display_parts.py
"""
import json
import os

FOLDER = "resources/library/display"
SEGMENTS = ["a", "b", "c", "d", "e", "f", "g"]
# Segments lit for the digits 0-9 (codes 10-15 are blank).
DIGITS = {
    0: "abcdef", 1: "bc", 2: "abdeg", 3: "abcdg", 4: "bcfg",
    5: "acdfg", 6: "acdefg", 7: "abc", 8: "abcdefg", 9: "abcdfg",
}
LED_MODELS = [
    ".model LED_red D(IS=1.2e-18 N=2 RS=5 BV=5 IBV=10u)",
    ".model LED_yellow D(IS=1.7e-19 N=2 RS=5 BV=5 IBV=10u)",
    ".model LED_green D(IS=2.5e-20 N=2 RS=5 BV=5 IBV=10u)",
    ".model LED_blue D(IS=6.7e-28 N=2 RS=8 BV=5 IBV=10u)",
    ".model LED_white D(IS=6.7e-28 N=2 RS=8 BV=5 IBV=10u)",
]


def horizontal(y):
    return {"polygon": [-1.2, y, -0.95, y - 0.25, 0.95, y - 0.25, 1.2, y, 0.95, y + 0.25, -0.95, y + 0.25]}


def vertical(x, top, bottom):
    return {"polygon": [x, top, x + 0.25, top + 0.25, x + 0.25, bottom - 0.25, x, bottom, x - 0.25, bottom - 0.25,
                        x - 0.25, top + 0.25]}


def display(common_anode):
    kind = "common anode" if common_anode else "common cathode"
    graphics = [{"rect": [-3, -5, 6, 10]}]
    shapes = {
        "a": horizontal(-3.4),
        "b": vertical(1.45, -3.15, -0.25),
        "c": vertical(1.45, 0.25, 3.15),
        "d": horizontal(3.4),
        "e": vertical(-1.45, 0.25, 3.15),
        "f": vertical(-1.45, -3.15, -0.25),
        "g": horizontal(0),
    }
    index = {}
    for s in SEGMENTS:
        index[s] = len(graphics)
        graphics.append(shapes[s])
    index["dp"] = len(graphics)
    graphics.append({"circle": [2.1, 3.4, 0.3]})
    pins = []
    left = ["a", "b", "c", "d"]
    right = ["e", "f", "g", "dp"]
    for i, name in enumerate(left):
        y = -3 + 2 * i
        pins.append({"name": name, "at": [-5, y], "dir": "left"})
        graphics.append({"line": [-5, y, -3, y]})
        graphics.append({"text": name, "at": [-2.5, y], "size": 0.6})
    for i, name in enumerate(right):
        y = -3 + 2 * i
        pin = {"name": name, "at": [5, y], "dir": "right"}
        if name == "dp":
            pin["optional"] = True
        pins.append(pin)
        graphics.append({"line": [3, y, 5, y]})
        graphics.append({"text": name, "at": [2.5, y], "size": 0.6})
    pins.append({"name": "COM", "at": [0, 7], "dir": "down"})
    graphics.append({"line": [0, 5, 0, 7]})
    graphics.append({"text": "CA" if common_anode else "CC", "at": [0, 4.4], "size": 0.6})
    spice = []
    lights = []
    for s in SEGMENTS + ["dp"]:
        pin = s
        source = f"V_{{ref}}_s{s}"
        node = f"s{s}_{{ref}}"
        if common_anode:  # COM -> source -> LED -> segment pin
            spice += [f"{source} {{node:COM}} {node} DC 0", f"D_{{ref}}_{s} {node} {{node:{pin}}} LED_{{color}}"]
        else:  # segment pin -> source -> LED -> COM
            spice += [f"{source} {{node:{pin}}} {node} DC 0", f"D_{{ref}}_{s} {node} {{node:COM}} LED_{{color}}"]
        lights.append({"element": source, "graphic": index[s]})
    return {
        "type": "seven_segment_ca" if common_anode else "seven_segment_cc",
        "name": f"7-segment display ({kind})",
        "category": "Displays",
        "prefix": "DS",
        "description": f"7-segment LED display, {kind}: segments a-g and the decimal point are LEDs "
                       + ("from COM to each segment pin (a low segment pin lights it, like the 7447 drives)"
                          if common_anode else "from each segment pin to COM (a high segment pin lights it, like the "
                                               "4511 drives)")
                       + "; each segment lights with its own current, fully at 10 mA; use a resistor per segment",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-3, -6],
        "value_offset": [3, 6],
        "value_parameter": "color",
        "parameters": [{"name": "color", "label": "Color", "unit": "", "default": "red",
                        "description": "Segment color; sets the forward voltage (red about 1.9 V at 10 mA)",
                        "choices": ["red", "yellow", "green", "blue", "white"]}],
        "lights": {"full": "10m", "color_parameter": "color", "segments": lights},
        "spice": spice,
        "models": LED_MODELS,
    }


def level(node):
    return f"0.5*(1+tanh({{= 40 / VDD}}*(V({node})-{{= VDD / 2}})))"


def decoder(active_low):
    inputs = ["A", "B", "C", "D"]  # A is the least significant bit
    spice = []
    for i, name in enumerate(inputs):
        spice.append(f"B_{{ref}}_l{i} l{i}_{{ref}} 0 V={level('{node:' + name + '}')}")
    for digit in range(10):
        factors = []
        for i in range(4):
            bit = (digit >> i) & 1
            factors.append(f"V(l{i}_{{ref}})" if bit else f"(1-V(l{i}_{{ref}}))")
        spice.append(f"B_{{ref}}_m{digit} m{digit}_{{ref}} 0 V={'*'.join(factors)}")
    for s in SEGMENTS:
        on = "+".join(f"V(m{d}_{{ref}})" for d, lit in DIGITS.items() if s in lit)
        value = f"(1-min({on},1))" if active_low else f"min({on},1)"
        spice += [
            f"B_{{ref}}_x{s} x{s}_{{ref}} 0 V={{VDD}}*{value}",
            f"R_{{ref}}_d{s} x{s}_{{ref}} y{s}_{{ref}} 1k",
            f"C_{{ref}}_d{s} y{s}_{{ref}} 0 10p",
            f"B_{{ref}}_o{s} z{s}_{{ref}} 0 V=V(y{s}_{{ref}})",
            f"R_{{ref}}_o{s} z{s}_{{ref}} {{node:{s}}} 50",
        ]
    spice += [f"R_{{ref}}_i{i} {{node:{name}}} 0 1G" for i, name in enumerate(inputs)]
    graphics = [{"rect": [-3, -8, 6, 16]}, {"text": "7447" if active_low else "4511", "at": [0, -8.8], "size": 0.8}]
    pins = []
    for i, name in enumerate(inputs):
        y = -3 + 2 * i
        graphics += [{"line": [-5, y, -3, y]}, {"text": name, "at": [-2.4, y], "size": 0.7}]
        pins.append({"name": name, "at": [-5, y], "dir": "left"})
    for i, s in enumerate(SEGMENTS):
        y = -6 + 2 * i
        graphics += [{"line": [3, y, 5, y]}, {"text": s, "at": [2.4, y], "size": 0.7}]
        if active_low:
            graphics.append({"circle": [3.35, y, 0.3]})
        pins.append({"name": s, "at": [5, y], "dir": "right"})
    return {
        "type": "decoder_7447" if active_low else "decoder_4511",
        "name": "BCD to 7-segment decoder (7447)" if active_low else "BCD to 7-segment decoder (4511)",
        "category": "Displays",
        "prefix": "U",
        "description": ("BCD to 7-segment decoder like the 7447: outputs low light the segments of a common-anode "
                        "display" if active_low else
                        "BCD to 7-segment decoder like the 4511: outputs high light the segments of a common-cathode "
                        "display")
                       + "; inputs D C B A (A least significant) show 0-9, codes 10-15 are blank; behavioral, on a "
                         "hidden supply VDD like the logic gates (no lamp test, blanking or latch inputs)",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-3, -10],
        "value_offset": [-3, 9],
        "value_parameter": "VDD",
        "parameters": [{"name": "VDD", "label": "Supply", "unit": "V", "default": "5",
                        "description": "Logic supply: output high level and input threshold (VDD/2)",
                        "min": "1.5", "max": "15"}],
        "spice": spice,
    }


def main():
    os.makedirs(FOLDER, exist_ok=True)
    for p in [display(False), display(True), decoder(False), decoder(True)]:
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
