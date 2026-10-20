"""Writes the adjustable and controlled sources of resources/library/sources
(SN-209).

Adjustable sources are turned while running like a potentiometer: their value
goes from the lower to the upper limit with the position. Controlled sources
are ngspice's linear E, G, F and H elements; a current-controlled source senses
the current from C+ to C- through a 0 V source, so its control pins are a
short circuit, like an ammeter.

Run from the repository root: python tools/generate_source_parts.py
"""
import json
import os

FOLDER = "resources/library/sources"
CATEGORY = "Sources"


def number(name, label, unit, default, description, minimum=None, maximum=None):
    p = {"name": name, "label": label, "unit": unit, "default": default, "description": description}
    if minimum is not None:
        p["min"] = minimum
    if maximum is not None:
        p["max"] = maximum
    return p


POSITION = number("position", "Position", "%", "50",
                  "Where the value starts, from the lower (0 %) to the upper limit (100 %); drag the source or use "
                  "the wheel while running", "0", "100")

# A diagonal arrow across the source's circle: the usual mark of an adjustable part.
ADJUST_ARROW = [
    {"line": [-1.8, 1.8, 1.6, -1.6]},
    {"polygon": [1.9, -1.9, 1.0, -1.6, 1.6, -1.0], "fill": True},
]


def adjustable_voltage():
    return {
        "type": "vadj",
        "name": "Adjustable voltage source",
        "category": CATEGORY,
        "prefix": "V",
        "description": "Voltage between a lower and an upper limit, turned while running",
        "symbol": {
            "graphics": [
                {"circle": [0, 0, 1.5]},
                {"line": [0, -3, 0, -1.5]},
                {"line": [0, 1.5, 0, 3]},
                {"line": [-0.4, -0.8, 0.4, -0.8]},
                {"line": [0, -1.2, 0, -0.4]},
                {"line": [-0.4, 0.8, 0.4, 0.8]},
            ] + ADJUST_ARROW,
            "pins": [
                {"name": "+", "at": [0, -3], "dir": "up"},
                {"name": "-", "at": [0, 3], "dir": "down"},
            ],
        },
        "parameters": [
            number("Vmin", "Lower limit", "V", "0", "Voltage at 0 %"),
            number("Vmax", "Upper limit", "V", "5", "Voltage at 100 %"),
            POSITION,
        ],
        "interactive": {"kind": "potentiometer", "parameter": "position"},
        "readout": {"kind": "voltage", "pins": ["+", "-"]},
        "spice": [
            "B_{ref} {node:+} {node:-} V={Vmin}+({Vmax}-{Vmin})*V({control})",
        ],
    }


def adjustable_current():
    return {
        "type": "iadj",
        "name": "Adjustable current source",
        "category": CATEGORY,
        "prefix": "I",
        "description": "Current between a lower and an upper limit, turned while running",
        "symbol": {
            "graphics": [
                {"circle": [0, 0, 1.5]},
                {"line": [0, -3, 0, -1.5]},
                {"line": [0, 1.5, 0, 3]},
                {"line": [0, 0.9, 0, -0.6]},
                {"polygon": [0, -1, -0.4, -0.3, 0.4, -0.3], "fill": True},
            ] + ADJUST_ARROW,
            "pins": [
                {"name": "+", "at": [0, -3], "dir": "up"},
                {"name": "-", "at": [0, 3], "dir": "down"},
            ],
        },
        "parameters": [
            number("Imin", "Lower limit", "A", "0", "Current out of + at 0 %"),
            number("Imax", "Upper limit", "A", "10m", "Current out of + at 100 %"),
            POSITION,
        ],
        "interactive": {"kind": "potentiometer", "parameter": "position"},
        "readout": {"kind": "current", "element": "V_{ref}_m"},
        "spice": [
            "B_{ref} {node:-} m_{ref} I={Imin}+({Imax}-{Imin})*V({control})",
            "V_{ref}_m m_{ref} {node:+} DC 0",
        ],
    }


def controlled(type_, name, prefix, current_control, current_output, gain, description):
    """Control pins C+ and C- on the left, output + and - on the right."""
    # One outline holds the control port and the output, so the control pins read as part of the source.
    graphics = [
        {"rect": [-2, -2.4, 6, 4.8]},
        {"polygon": [2, -1.6, 3.6, 0, 2, 1.6, 0.4, 0]},
        {"line": [2, -3, 2, -2.4]},
        {"line": [2, -2.4, 2, -1.6]},
        {"line": [2, 1.6, 2, 2.4]},
        {"line": [2, 2.4, 2, 3]},
        {"line": [-3, -2, -2, -2]},
        {"line": [-3, 2, -2, 2]},
    ]
    if current_output:
        graphics += [{"line": [2, 0.9, 2, -0.6]}, {"polygon": [2, -1, 1.6, -0.3, 2.4, -0.3], "fill": True}]
    else:
        graphics += [{"line": [1.6, -0.6, 2.4, -0.6]}, {"line": [2, -1, 2, -0.2]}, {"line": [1.6, 0.7, 2.4, 0.7]}]
    if current_control:
        # The sensed path between C+ and C-, with its direction.
        graphics += [{"line": [-2, -2, -0.8, -2]}, {"line": [-0.8, -2, -0.8, 2]}, {"line": [-0.8, 2, -2, 2]},
                     {"polygon": [-0.8, 0.4, -1.2, -0.3, -0.4, -0.3], "fill": True}]
    else:
        graphics += [{"text": "+", "at": [-1.3, -1.6], "size": 0.8}, {"text": "−", "at": [-1.3, 1.6], "size": 0.8}]
    node = "{node:C+} {node:C-}"
    if current_control:
        spice = ["V_{ref}_s {node:C+} {node:C-} DC 0"]
        control = "V_{ref}_s"
    else:
        spice = ["R_{ref}_p {node:C+} 0 1G", "R_{ref}_n {node:C-} 0 1G"]
        control = node
    if current_output:
        spice.append(f"{prefix}_{{ref}} {{node:-}} {{node:+}} {control} {{{gain['name']}}}")
    else:
        spice.append(f"{prefix}_{{ref}} {{node:+}} {{node:-}} {control} {{{gain['name']}}}")
    return {
        "type": type_,
        "name": name,
        "category": CATEGORY,
        "prefix": prefix,
        "description": description,
        "symbol": {
            "graphics": graphics,
            "pins": [
                {"name": "C+", "at": [-3, -2], "dir": "left"},
                {"name": "C-", "at": [-3, 2], "dir": "left"},
                {"name": "+", "at": [2, -3], "dir": "up"},
                {"name": "-", "at": [2, 3], "dir": "down"},
            ],
        },
        "designator_offset": [4, -3],
        "value_offset": [4, 3],
        "value_parameter": gain["name"],
        "parameters": [gain],
        "spice": spice,
    }


def build():
    return [
        adjustable_voltage(),
        adjustable_current(),
        controlled("vcvs", "Voltage-controlled voltage source", "E", False, False,
                   number("gain", "Gain", "V/V", "10", "Output voltage per volt between C+ and C-"),
                   "Output voltage = gain x voltage between C+ and C-"),
        controlled("vccs", "Voltage-controlled current source", "G", False, True,
                   number("gm", "Transconductance", "S", "1m", "Output current per volt between C+ and C-"),
                   "Output current = transconductance x voltage between C+ and C-"),
        controlled("cccs", "Current-controlled current source", "F", True, True,
                   number("gain", "Gain", "A/A", "100", "Output current per ampere flowing from C+ to C-"),
                   "Output current = gain x current from C+ to C-"),
        controlled("ccvs", "Current-controlled voltage source", "H", True, False,
                   number("rm", "Transresistance", "Ohm", "1k", "Output voltage per ampere flowing from C+ to C-"),
                   "Output voltage = transresistance x current from C+ to C-"),
    ]


def main():
    os.makedirs(FOLDER, exist_ok=True)
    # Short descriptions for the parts tree (details belong in the user guide).
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "part_descriptions.json"), encoding="utf-8") as fh:
        short = json.load(fh)
    for p in build():
        p["description"] = short.get(p["type"], p["description"])
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
