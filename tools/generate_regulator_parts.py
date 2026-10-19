"""Writes the behavioral voltage regulators and references of
resources/library/regulators (SN-201).

Linear regulators pass current from IN to OUT (so the input current is the
load current plus a quiescent current): a transconductance of 100 S pulls the
output toward its set voltage, limited by the dropout voltage below IN and by
the current limit. The TL431 is a shunt reference: current from K to A grows
with 10 S per volt that REF is above Vref. The command of each part goes through
an internal node with a 1 us time constant (an error amplifier's dominant
pole), and a small capacitor sits on each output: with a direct high-gain loop
ngspice cannot take the first step of a run that starts from 0 V.

Run from the repository root: python tools/generate_regulator_parts.py
"""
import json
import os

FOLDER = "resources/library/regulators"
CATEGORY = "Integrated circuits"


def number(name, label, unit, default, description, minimum, maximum):
    return {"name": name, "label": label, "unit": unit, "default": default, "description": description,
            "min": minimum, "max": maximum}


def box(title, bottom):
    """IN on the left, OUT on the right, `bottom` (GND or ADJ) below."""
    return {
        "graphics": [
            {"rect": [-3, -2, 6, 4]},
            {"line": [-5, 0, -3, 0]},
            {"line": [3, 0, 5, 0]},
            {"line": [0, 2, 0, 4]},
            {"text": "IN", "at": [-2, 0], "size": 0.7},
            {"text": "OUT", "at": [1.8, 0], "size": 0.7},
            {"text": bottom, "at": [0, 1.3], "size": 0.7},
            {"text": title, "at": [0, -1.1], "size": 0.8},
        ],
        "pins": [
            {"name": "IN", "at": [-5, 0], "dir": "left"},
            {"name": "OUT", "at": [5, 0], "dir": "right"},
            {"name": bottom, "at": [0, 4], "dir": "down"},
        ],
    }


def pass_element(target_error, dropout, limit):
    """Current from IN to OUT toward the set voltage, within dropout and limit.

    The command goes through an internal node with a 1 us time constant (the
    error amplifier's dominant pole); a direct high-gain loop cannot start
    from 0 V in ngspice.
    """
    command = (f"min(max(min(100*({target_error}),100*(V({{node:IN}},{{node:OUT}})-{{{dropout}}})),0),"
               f"{{{limit}}})")
    return [
        f"B_{{ref}}_c 0 c_{{ref}} I=1m*({command}-V(c_{{ref}}))",
        "C_{ref}_c c_{ref} 0 1n",
        "B_{ref}_p {node:IN} {node:OUT} I=max(V(c_{ref}),0)",
    ]


def fixed(type_, name, title, vout, dropout, limit, quiescent, description):
    return {
        "type": type_,
        "name": name,
        "category": CATEGORY,
        "prefix": "U",
        "description": description + "; behavioral: no thermal shutdown, no ripple rejection limit, no "
                       "capacitors needed for stability",
        "symbol": box(title, "GND"),
        "designator_offset": [-3, -4],
        "value_offset": [3, 3],
        "value_parameter": "Vout",
        "parameters": [
            number("Vout", "Output voltage", "V", vout, "Regulated output voltage (5 for a 7805, 12 for a 7812)",
                   "0.5", "40"),
            number("Vdo", "Dropout voltage", "V", dropout, "Smallest IN-OUT difference that still regulates",
                   "0.05", "5"),
            number("Ilim", "Current limit", "A", limit, "Largest output current", "0.001", "10"),
            number("Iq", "Quiescent current", "A", quiescent, "Current from IN to GND for the regulator itself",
                   "0", "0.1"),
        ],
        "spice": [
            *pass_element("{Vout}-V({node:OUT},{node:GND})", "Vdo", "Ilim"),
            "B_{ref}_q {node:IN} {node:GND} I={Iq}*tanh(max(V({node:IN},{node:GND}),0))",
            "R_{ref}_o {node:OUT} {node:GND} 1meg",
            "C_{ref}_o {node:OUT} {node:GND} 100n",
        ],
    }


def lm317():
    return {
        "type": "lm317",
        "name": "Adjustable regulator (LM317)",
        "category": CATEGORY,
        "prefix": "U",
        "description": "Adjustable positive regulator like the LM317: keeps OUT 1.25 V above ADJ (Vout = 1.25 V x "
                       "(1 + R2/R1) with R1 from OUT to ADJ and R2 from ADJ to ground), 50 uA flow out of ADJ; "
                       "behavioral: no minimum load, no thermal shutdown",
        "symbol": box("LM317", "ADJ"),
        "designator_offset": [-3, -4],
        "value_offset": [3, 3],
        "parameters": [
            number("Vref", "Reference voltage", "V", "1.25", "Voltage between OUT and ADJ", "1", "1.5"),
            number("Vdo", "Dropout voltage", "V", "2", "Smallest IN-OUT difference that still regulates", "0.05",
                   "5"),
            number("Ilim", "Current limit", "A", "1.5", "Largest output current", "0.001", "10"),
        ],
        "spice": [
            *pass_element("{Vref}-V({node:OUT},{node:ADJ})", "Vdo", "Ilim"),
            "B_{ref}_a {node:IN} {node:ADJ} I=50u*tanh(max(V({node:IN},{node:OUT}),0))",
            "R_{ref}_o {node:OUT} {node:ADJ} 10meg",
            "C_{ref}_o {node:OUT} {node:ADJ} 100n",
        ],
    }


def tl431():
    zener = {
        "graphics": [
            {"polygon": [-1, 0.8, 1, 0.8, 0, -0.8]},
            {"line": [-1, -0.8, 1, -0.8]},
            {"line": [-1, -0.8, -1.4, -0.4]},
            {"line": [1, -0.8, 1.4, -1.2]},
            {"line": [0, -3, 0, -0.8]},
            {"line": [0, 0.8, 0, 3]},
            {"line": [-3, 0, -0.5, 0]},
        ],
        "pins": [
            {"name": "K", "at": [0, -3], "dir": "up"},
            {"name": "A", "at": [0, 3], "dir": "down"},
            {"name": "REF", "at": [-3, 0], "dir": "left"},
        ],
    }
    return {
        "type": "tl431",
        "name": "Shunt reference (TL431)",
        "category": CATEGORY,
        "prefix": "U",
        "description": "Adjustable shunt reference like the TL431: sinks current from K to A until REF is Vref "
                       "above A (2.495 V; with REF on K it is a 2.5 V reference, with a divider Vk = Vref x (1 + "
                       "R1/R2)); feed it through a resistor; behavioral: no minimum cathode current",
        "symbol": zener,
        "designator_offset": [2, -2],
        "value_offset": [2, 2],
        "value_parameter": "Vref",
        "parameters": [
            number("Vref", "Reference voltage", "V", "2.495", "REF voltage the part regulates to, against A",
                   "1", "5"),
        ],
        "spice": [
            "B_{ref}_c 0 c_{ref} I=1m*(max(10*(V({node:REF},{node:A})-{Vref}),0)*tanh(max(V({node:K},{node:A}),0)/0.5)-V(c_{ref}))",
            "C_{ref}_c c_{ref} 0 1n",
            "B_{ref} {node:K} {node:A} I=max(V(c_{ref}),0)",
            "R_{ref}_r {node:REF} {node:A} 100meg",
            "R_{ref}_k {node:K} {node:A} 100meg",
            "C_{ref}_k {node:K} {node:A} 10n",
        ],
    }


def build():
    return [
        fixed("regulator_78xx", "Voltage regulator (78xx)", "78xx", "5", "2", "1", "5m",
              "Fixed positive regulator like the 7805 (set Vout for 7809, 7812...): OUT stays at Vout above GND "
              "while IN is at least Vdo higher; the input current is the load current plus about 5 mA"),
        fixed("regulator_ldo", "Low-dropout regulator (LDO)", "LDO", "3.3", "1.1", "0.8", "5m",
              "Fixed low-dropout regulator like the LM1117-3.3 or AMS1117: 3.3 V from 5 V or from a USB supply; "
              "regulates down to about 1.1 V of headroom"),
        lm317(),
        tl431(),
    ]


def main():
    os.makedirs(FOLDER, exist_ok=True)
    for p in build():
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
