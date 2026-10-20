"""Writes the A/D and D/A converters of resources/library/converters (SN-211).

Ideal converters with 4, 8, 10 or 12 bits, in the style of the logic parts: a
hidden supply VDD sets the logic levels (inputs switch at VDD/2, outputs go to
VDD), and the reference is a pin, so the full scale follows the circuit.

- ADC: converts continuously (no clock, no sampling): the code is
  floor(2^N x VIN / VREF), limited to 0..2^N-1, on D0 (LSB) to D(N-1); each
  output goes through 1 kOhm and 10 pF and 50 Ohm, like a logic gate.
- DAC: OUT = VREF x code / 2^N from D0 (LSB) to D(N-1), behind 10 Ohm.

Run from the repository root: python tools/generate_converter_parts.py
"""
import json
import os

FOLDER = "resources/library/converters"
CATEGORY = "Converters"
BITS = [4, 8, 10, 12]

VDD = {"name": "VDD", "label": "Supply", "unit": "V", "default": "5",
       "description": "Logic supply: 5 V for 74HC, 3.3 V next to a Blue Pill; output high level and input threshold "
                      "(VDD/2)", "min": "1.5", "max": "15"}


def level(node):
    """0..1 for a logic input, switching at VDD/2."""
    return f"0.5*(1+tanh({{= 40 / VDD}}*(V({node})-{{= VDD / 2}})))"


def box(title, bits, left):
    """Data pins D0 (top) .. D(N-1) on the right; `left` pins on the left from the top."""
    height = max(2 * bits, 2 * len(left)) + 2
    top = -height // 2
    graphics = [{"rect": [-3, top, 6, height]}, {"text": title, "at": [0, top - 0.8], "size": 0.8}]
    pins = []
    for i, name in enumerate(left):
        y = top + 2 + 2 * i
        graphics += [{"line": [-5, y, -3, y]}, {"text": name, "at": [-1.7, y], "size": 0.7}]
        pins.append({"name": name, "at": [-5, y], "dir": "left"})
    for k in range(bits):
        y = top + 2 + 2 * k
        name = f"D{k}"
        graphics += [{"line": [3, y, 5, y]}, {"text": name, "at": [1.9, y], "size": 0.7}]
        pins.append({"name": name, "at": [5, y], "dir": "right"})
    return {"graphics": graphics, "pins": pins}, top, height


def adc(bits):
    symbol, top, height = box(f"ADC{bits}", bits, ["VIN", "VREF"])
    full = 2 ** bits
    x = f"min(max(V({{node:VIN}})/max(V({{node:VREF}}),1e-6)*{full},0),{full - 1})"
    spice = [f"B_{{ref}}_x x_{{ref}} 0 V={x}"]
    for k in range(bits):
        bit = f"floor(V(x_{{ref}})/{2 ** k})-2*floor(V(x_{{ref}})/{2 ** (k + 1)})"
        spice += [
            f"B_{{ref}}_b{k} b{k}_{{ref}} 0 V={{VDD}}*({bit})",
            f"R_{{ref}}_d{k} b{k}_{{ref}} y{k}_{{ref}} 1k",
            f"C_{{ref}}_d{k} y{k}_{{ref}} 0 10p",
            f"B_{{ref}}_o{k} z{k}_{{ref}} 0 V=V(y{k}_{{ref}})",
            f"R_{{ref}}_o{k} z{k}_{{ref}} {{node:D{k}}} 50",
        ]
    spice += ["R_{ref}_in {node:VIN} 0 1G", "R_{ref}_ref {node:VREF} 0 1G"]
    return {
        "type": f"adc_{bits}",
        "name": f"A/D converter {bits}-bit",
        "category": CATEGORY,
        "prefix": "U",
        "description": f"Ideal {bits}-bit A/D converter: D0-D{bits - 1} = 2^{bits} x VIN / VREF, converting "
                       "continuously",
        "symbol": symbol,
        "designator_offset": [-3, top - 2],
        "value_offset": [-3, top + height + 1],
        "value_parameter": "VDD",
        "parameters": [VDD],
        "spice": spice,
    }


def dac(bits):
    symbol, top, height = box(f"DAC{bits}", bits, ["VREF", "OUT"])
    # The data pins are inputs on the right: mirror the box so they enter from the left.
    for p in symbol["pins"]:
        p["at"][0] = -p["at"][0]
        p["dir"] = "right" if p["dir"] == "left" else "left"
    for g in symbol["graphics"]:
        if "line" in g:
            g["line"] = [-g["line"][0], g["line"][1], -g["line"][2], g["line"][3]]
        elif "text" in g and g["at"][0] != 0:
            g["at"] = [-g["at"][0], g["at"][1]]
    code = "+".join(f"{2 ** k}*{level(f'{{node:D{k}}}')}" for k in range(bits))
    spice = [
        f"B_{{ref}} o_{{ref}} 0 V=V({{node:VREF}})*({code})/{2 ** bits}",
        "R_{ref}_o o_{ref} {node:OUT} 10",
        "R_{ref}_ref {node:VREF} 0 1G",
    ] + [f"R_{{ref}}_i{k} {{node:D{k}}} 0 1G" for k in range(bits)]
    return {
        "type": f"dac_{bits}",
        "name": f"D/A converter {bits}-bit",
        "category": CATEGORY,
        "prefix": "U",
        "description": f"Ideal {bits}-bit D/A converter: OUT = VREF x code / 2^{bits} from D0-D{bits - 1}",
        "symbol": symbol,
        "designator_offset": [-3, top - 2],
        "value_offset": [-3, top + height + 1],
        "value_parameter": "VDD",
        "parameters": [VDD],
        "spice": spice,
    }


def main():
    os.makedirs(FOLDER, exist_ok=True)
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "part_descriptions.json"), encoding="utf-8") as fh:
        short = json.load(fh)
    for p in [adc(b) for b in BITS] + [dac(b) for b in BITS]:
        p["description"] = short.get(p["type"], p["description"])
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
