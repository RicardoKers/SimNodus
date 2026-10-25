"""Writes the external I/O parts of resources/library/io (SN-218, ADR 0104).

They exchange signals with an external program, such as a virtual industrial
plant, through the external I/O link (Simulate > External I/O). Each part has
a free-text address that is sent as written ("%IX0.10"):

- Bit input: the external value drives the pin (0 V or the high level); a
  toggle, so it can be operated by hand while nothing is connected.
- Word input: the external number drives the pin, scaled from a word range to
  a voltage range (0-1000 to 0-3.3 V by default); a potentiometer.
- Bit output: the pin's voltage, above or below a threshold, is sent.
- Word output: the pin's voltage, scaled to the word range, is sent.

Outputs only sense their pin (1 GOhm to ground). Polarity inverts a bit on
the way in or out; the link applies it, so the parts' netlists stay simple.

Run from the repository root: python tools/generate_io_parts.py
"""
import json
import os

FOLDER = "resources/library/io"
CATEGORY = "External I/O"


def number(name, label, unit, default, description, minimum=None, maximum=None):
    p = {"name": name, "label": label, "unit": unit, "default": default, "description": description}
    if minimum is not None:
        p["min"] = minimum
    if maximum is not None:
        p["max"] = maximum
    return p


def address(default):
    return {"name": "address", "label": "Address", "kind": "text", "default": default,
            "description": "Name of the signal in the external program, sent exactly as written (%IX0.10, %QW2)"}


POLARITY = {"name": "polarity", "label": "Polarity", "choices": ["normal", "inverted"], "default": "normal",
            "description": "Inverted: true outside is a low level in the circuit"}
WORD_RANGE = [
    number("Wmin", "Word at the lower voltage", "", "0", "Number that stands for the lower voltage"),
    number("Wmax", "Word at the upper voltage", "", "1000", "Number that stands for the upper voltage"),
    number("Vmin", "Lower voltage", "V", "0", "Voltage of the lower word"),
    number("Vmax", "Upper voltage", "V", "3.3", "Voltage of the upper word"),
]


def symbol(label, output):
    """A box with the label and one pin; inputs drive their pin, outputs read it."""
    arrow = ([{"polygon": [-1.6, -0.5, -0.8, 0, -1.6, 0.5], "fill": True}] if not output
             else [{"polygon": [-0.8, -0.5, -1.6, 0, -0.8, 0.5], "fill": True}])
    return {
        "graphics": [
            {"rect": [-6, -1, 6, 2]},
            {"text": label, "at": [-3.6, 0], "size": 0.7},
            {"line": [0, 0, 2, 0]},
        ] + arrow,
        "pins": [{"name": "1", "at": [2, 0], "dir": "right"}],
    }


SENSE = ["R_{ref} {node:1} 0 1e9"]


def build():
    return [
        {
            "type": "io_bit_in",
            "name": "Bit input",
            "category": CATEGORY,
            "prefix": "IO",
            "description": "A true/false signal from the external program drives the pin; click it to change it by hand",
            "symbol": symbol("BIT IN", False),
            "designator_offset": [-6, -2],
            "value_offset": [-6, 2],
            "value_parameter": "address",
            "parameters": [
                address("%IX0.0"),
                POLARITY,
                number("Vhigh", "High level", "V", "3.3", "Voltage of a true bit"),
                {"name": "state", "label": "Initial state", "choices": ["0", "1"], "default": "0",
                 "description": "Level until the external program sends a value"},
            ],
            "interactive": {"kind": "toggle", "parameter": "state"},
            "spice": ["B_{ref} {node:1} 0 V={Vhigh}*V({control})"],
        },
        {
            "type": "io_word_in",
            "name": "Word input",
            "category": CATEGORY,
            "prefix": "IO",
            "description": "A number from the external program drives the pin, scaled to a voltage",
            "symbol": symbol("WORD IN", False),
            "designator_offset": [-6, -2],
            "value_offset": [-6, 2],
            "value_parameter": "address",
            "parameters": [address("%IW0")] + WORD_RANGE + [
                number("position", "Initial value", "%", "0",
                       "Where the value starts, from the lower (0 %) to the upper voltage (100 %); drag it to change "
                       "it by hand", "0", "100"),
            ],
            "interactive": {"kind": "potentiometer", "parameter": "position"},
            "spice": ["B_{ref} {node:1} 0 V={Vmin}+({Vmax}-{Vmin})*V({control})"],
        },
        {
            "type": "io_bit_out",
            "name": "Bit output",
            "category": CATEGORY,
            "prefix": "IO",
            "description": "Sends whether the pin's voltage is above the threshold to the external program",
            "symbol": symbol("BIT OUT", True),
            "designator_offset": [-6, -2],
            "value_offset": [-6, 2],
            "value_parameter": "address",
            "parameters": [
                address("%QX0.0"),
                POLARITY,
                number("Vth", "Threshold", "V", "1.65", "A voltage above it is a true bit"),
            ],
            "spice": SENSE,
        },
        {
            "type": "io_word_out",
            "name": "Word output",
            "category": CATEGORY,
            "prefix": "IO",
            "description": "Sends the pin's voltage, scaled to a whole number, to the external program",
            "symbol": symbol("WORD OUT", True),
            "designator_offset": [-6, -2],
            "value_offset": [-6, 2],
            "value_parameter": "address",
            "parameters": [address("%QW0")] + WORD_RANGE,
            "spice": SENSE,
        },
    ]


def main():
    os.makedirs(FOLDER, exist_ok=True)
    for p in build():
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
