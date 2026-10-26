"""Writes the motion parts of resources/library/motion (SN-226).

- L298 dual full bridge (Multiwatt15 pins): each output switches to VS or to
  its SENS pin through about 1.3 ohm (the datasheet's saturation voltages at
  1 A), or floats while its enable is low; TTL inputs; no internal flyback
  diodes, as on the chip: add them.
- ULN2003 seven Darlington sinks (DIP-16) with flyback diodes to COM.
- Stepper motors: bipolar (two coils, 200 steps per revolution) and the
  unipolar 28BYJ-48 (center-tapped coils on COM, 64:1 gear, 2048 steps per
  revolution in full steps). The rotor's electrical angle follows the coils'
  field like a magnet with viscous friction: d(theta)/dt = k (Ib cos theta -
  Ia sin theta); back EMF and inertia are left out. The shaft angle is shown
  while running.
- Rotary encoder (EC11-like) with contacts A and B to C: one quadrature cycle
  per detent, 20 detents per turn; turned while running like a potentiometer
  (each wheel step is one detent).

Run from the repository root: python tools/generate_motion_parts.py
"""
import json
import os

FOLDER = "resources/library/motion"
CATEGORY = "Motors & motion"


def number(name, label, unit, default, description, minimum=None, maximum=None):
    p = {"name": name, "label": label, "unit": unit, "default": default, "description": description}
    if minimum is not None:
        p["min"] = minimum
    if maximum is not None:
        p["max"] = maximum
    return p


def pin(name, x, y, direction, optional=False):
    p = {"name": name, "at": [x, y], "dir": direction}
    if optional:
        p["optional"] = True
    return p


def box(title, left, right, top=(), bottom=(), width=8):
    """A box with pins two units apart on its sides; top/bottom are (name, x)."""
    rows = max(len(left), len(right))
    height = 2 * rows + 2
    upper = -(height // 2)
    graphics = [{"rect": [-width / 2, upper, width, height]},
                {"text": title, "at": [0, upper + height / 2], "size": 0.8}]
    pins = []
    for names, x_tip, x_edge, direction in ((left, -width / 2 - 2, -width / 2, "left"),
                                            (right, width / 2 + 2, width / 2, "right")):
        first = -(len(names) - 1)
        for i, name in enumerate(names):
            y = first + 2 * i
            graphics.append({"line": [x_tip, y, x_edge, y]})
            graphics.append({"text": name, "at": [x_edge + (1.4 if x_edge < 0 else -1.4), y], "size": 0.55})
            pins.append(pin(name, x_tip, y, direction))
    for name, x in top:
        graphics += [{"line": [x, upper - 2, x, upper]}, {"text": name, "at": [x, upper + 0.6], "size": 0.5}]
        pins.append(pin(name, x, upper - 2, "up"))
    for name, x in bottom:
        graphics += [{"line": [x, -upper + 2, x, -upper]}, {"text": name, "at": [x, -upper - 0.6], "size": 0.5}]
        pins.append(pin(name, x, -upper + 2, "down"))
    return {"graphics": graphics, "pins": pins}, upper


def level(node, ground, threshold):
    return f"0.5*(1+tanh(10*(V({{node:{node}}},{{node:{ground}}})-{threshold})))"


def l298():
    left = ["IN1", "IN2", "ENA", "IN3", "IN4", "ENB"]
    right = ["OUT1", "OUT2", "OUT3", "OUT4", "SENSA", "SENSB"]
    symbol, upper = box("L298", left, right, top=[("VSS", -2), ("VS", 2)], bottom=[("GND", 0)], width=10)
    for p in symbol["pins"]:
        if p["name"] not in ("VS", "VSS", "GND"):
            p["optional"] = True  # a bridge may stay unused
    logic = "0.5*(1+tanh(5*(V({node:VSS},{node:GND})-4)))"
    spice = []
    for out, inp, en, sens in (("OUT1", "IN1", "ENA", "SENSA"), ("OUT2", "IN2", "ENA", "SENSA"),
                               ("OUT3", "IN3", "ENB", "SENSB"), ("OUT4", "IN4", "ENB", "SENSB")):
        e, x = level(en, "GND", 1.9), level(inp, "GND", 1.9)
        t = out.lower()
        spice += [
            f"B_{{ref}}_on{t} on{t}_{{ref}} 0 V={logic}*{e}",
            f"B_{{ref}}_x{t} x{t}_{{ref}} 0 V={x}",
            f"B_{{ref}}_h{t} {{node:VS}} {{node:{out}}} I=V(on{t}_{{ref}})*V(x{t}_{{ref}})*V({{node:VS}},{{node:{out}}})/1.3",
            f"B_{{ref}}_l{t} {{node:{out}}} {{node:{sens}}} I=V(on{t}_{{ref}})*(1-V(x{t}_{{ref}}))*V({{node:{out}}},{{node:{sens}}})/1.3",
            f"R_{{ref}}_f{t} {{node:{out}}} {{node:GND}} 10Meg",
        ]
    spice += [f"R_{{ref}}_i{p.lower()} {{node:{p}}} {{node:GND}} 1G" for p in left]
    spice += ["R_{ref}_qs {node:VS} {node:GND} 1k", "R_{ref}_ql {node:VSS} {node:GND} 220"]
    return {
        "type": "l298",
        "name": "Dual H-bridge L298",
        "category": CATEGORY,
        "prefix": "U",
        "description": "Dual full bridge for motors (L298): with ENA high, OUT1 follows IN1 and OUT2 follows IN2 "
                       "(high: to VS, low: to SENSA), likewise ENB, IN3, IN4 and OUT3, OUT4; an enable low lets its "
                       "outputs float (PWM on the enables sets the speed). About 1.3 ohm per switch, 2.6 V lost at "
                       "1 A; logic supply 5 V on VSS, motor supply up to 46 V on VS; tie SENSA and SENSB to GND or "
                       "through sense resistors. No internal flyback diodes: add them, as the datasheet says",
        "symbol": symbol,
        "designator_offset": [6, upper - 2],
        "value_offset": [6, -upper + 2],
        "spice": spice,
    }


def uln2003():
    pins = [f"IN{i}" for i in range(1, 8)] + ["GND", "COM"] + [f"OUT{i}" for i in range(7, 0, -1)]
    graphics = [{"rect": [-4, -8, 8, 16]}, {"arc": [0, -8, 0.7, 180, 180]},
                {"text": "ULN2003", "at": [0, 0], "size": 0.8}]
    result = []
    for i, name in enumerate(pins):
        if i < 8:
            x_tip, x_edge, y, direction = -6, -4, -7 + 2 * i, "left"
        else:
            x_tip, x_edge, y, direction = 6, 4, -7 + 2 * (15 - i), "right"
        graphics.append({"line": [x_tip, y, x_edge, y]})
        graphics.append({"text": name, "at": [x_edge + (1.4 if x_edge < 0 else -1.4), y], "size": 0.5})
        graphics.append({"text": str(i + 1), "at": [(x_tip + x_edge) / 2, y - 0.5], "size": 0.45})
        result.append(pin(name, x_tip, y, direction, optional=name.startswith(("IN", "OUT"))))
    spice = []
    for k in range(1, 8):
        spice += [
            f"B_{{ref}}_b{k} {{node:IN{k}}} {{node:GND}} I=max(V({{node:IN{k}}},{{node:GND}})-1.4,0)/2700",
            f"R_{{ref}}_i{k} {{node:IN{k}}} {{node:GND}} 1G",
            f"B_{{ref}}_s{k} s{k}_{{ref}} 0 V=0.5*(1+tanh(8*(V({{node:IN{k}}},{{node:GND}})-1.8)))",
            f"B_{{ref}}_o{k} {{node:OUT{k}}} {{node:GND}} I=V(s{k}_{{ref}})*max(V({{node:OUT{k}}},{{node:GND}})-0.8,0)/1.5",
            f"D_{{ref}}_d{k} {{node:OUT{k}}} {{node:COM}} DFLY_{{ref}}",
            f"R_{{ref}}_l{k} {{node:OUT{k}}} {{node:GND}} 100Meg",
        ]
    spice += [".model DFLY_{ref} D(IS=1e-12 N=1.8 RS=0.5)"]
    return {
        "type": "uln2003",
        "name": "Darlington array ULN2003",
        "category": CATEGORY,
        "prefix": "U",
        "description": "Seven Darlington sinks (ULN2003, DIP-16) for relays, lamps and unipolar steppers: an input "
                       "above about 1.8 V (2.7 kohm inside, so 3.3 V and 5 V logic both work) makes its output sink to "
                       "GND with about 0.9 V left; COM joins the flyback diodes of every output: tie it to the load's "
                       "supply",
        "symbol": {"graphics": graphics, "pins": result},
        "designator_offset": [-1, -10],
        "value_offset": [-1, 10],
        "spice": spice,
    }


def stepper_bipolar():
    graphics = [{"circle": [0, 0, 3]}, {"text": "M", "at": [0, -0.6], "size": 1.2},
                {"text": "step", "at": [0, 1.2], "size": 0.55},
                {"line": [-5, -2, -2.6, -2]}, {"line": [-5, 2, -2.6, 2]},
                {"line": [5, -2, 2.6, -2]}, {"line": [5, 2, 2.6, 2]},
                {"text": "A+", "at": [-4, -2.6], "size": 0.5}, {"text": "A-", "at": [-4, 2.6], "size": 0.5},
                {"text": "B+", "at": [4, -2.6], "size": 0.5}, {"text": "B-", "at": [4, 2.6], "size": 0.5}]
    pins = [pin("A+", -5, -2, "left"), pin("A-", -5, 2, "left"), pin("B+", 5, -2, "right"), pin("B-", 5, 2, "right")]
    return {
        "type": "stepper_bipolar",
        "name": "Stepper motor (bipolar)",
        "category": CATEGORY,
        "prefix": "M",
        "description": "Bipolar stepper motor like a NEMA 17: two coils (A+/A-, B+/B-) with their resistance and "
                       "inductance; the rotor turns one step for each step of the coil currents (full, half or "
                       "micro steps) and shows the shaft angle. Drive it with an H-bridge (L298) or a stepper driver; "
                       "behavioral: no back EMF or inertia, so it never loses steps",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-3, -5],
        "value_offset": [-3, 5],
        "value_parameter": "steps",
        "parameters": [
            number("R", "Coil resistance", "Ohm", "30", "Resistance of each coil", "0.1", "1k"),
            number("L", "Coil inductance", "H", "40m", "Inductance of each coil", "10u", "10"),
            number("Irated", "Rated current", "A", "400m", "Coil current for the full holding torque", "1m", "10"),
            number("steps", "Steps per revolution", "", "200", "Full steps per turn of the shaft", "4", "1000"),
        ],
        "readout": {"kind": "node", "element": "ang_{ref}", "scale": 1, "unit": "°"},
        "spice": coils("A+", "A-", "B+", "B-") + rotor("{= 2000 / Irated}", "{= steps / 4}"),
    }


def coils(a1, a2, b1, b2):
    return [
        f"V_{{ref}}_a {{node:{a1}}} ca_{{ref}} DC 0",
        "R_{ref}_ra ca_{ref} la_{ref} {R}",
        f"L_{{ref}}_a la_{{ref}} {{node:{a2}}} {{L}}",
        f"V_{{ref}}_b {{node:{b1}}} cb_{{ref}} DC 0",
        "R_{ref}_rb cb_{ref} lb_{ref} {R}",
        f"L_{{ref}}_b lb_{{ref}} {{node:{b2}}} {{L}}",
    ]


def rotor(k, cycles_per_turn, ia="I(V_{ref}_a)", ib="I(V_{ref}_b)"):
    """The electrical angle th (V = rad) follows the field; ang is the shaft angle in degrees."""
    return [
        f"B_{{ref}}_th 0 th_{{ref}} I={k}*({ib}*cos(V(th_{{ref}}))-{ia}*sin(V(th_{{ref}})))",
        "C_{ref}_th th_{ref} 0 1",
        "R_{ref}_th th_{ref} 0 1G",
        # Hundredths of a degree: no numerical dust on the symbol at rest.
        f"B_{{ref}}_ang ang_{{ref}} 0 V=floor(V(th_{{ref}})*5729.578/{cycles_per_turn}+0.5)/100",
    ]


def stepper_28byj48():
    graphics = [{"circle": [0, 0, 3]}, {"text": "M", "at": [0, -0.6], "size": 1.2},
                {"text": "28BYJ", "at": [0, 1.2], "size": 0.5}]
    pins = []
    for i, name in enumerate(["COM", "A", "B", "C", "D"]):
        y = -4 + 2 * i
        pins.append(pin(name, -6, y, "left"))
        graphics += [{"line": [-6, y, -3.4 if abs(y) < 3 else -2.6, y]},
                     {"text": name, "at": [-5, y - 0.6], "size": 0.5}]
    # Coil halves from COM: A and C on one phase (opposite field), B and D on the other.
    spice = []
    for name in "ABCD":
        n = name.lower()
        spice += [
            f"V_{{ref}}_{n} {{node:COM}} c{n}_{{ref}} DC 0",
            f"R_{{ref}}_r{n} c{n}_{{ref}} l{n}_{{ref}} {{R}}",
            f"L_{{ref}}_{n} l{n}_{{ref}} {{node:{name}}} {{L}}",
        ]
    ia = "(I(V_{ref}_a)-I(V_{ref}_c))"
    ib = "(I(V_{ref}_b)-I(V_{ref}_d))"
    return {
        "type": "stepper_28byj48",
        "name": "Stepper motor 28BYJ-48 (unipolar)",
        "category": CATEGORY,
        "prefix": "M",
        "description": "Small unipolar stepper motor with a 64:1 gear (28BYJ-48, 5 V): COM (red wire) to the supply "
                       "and the coil ends A, B, C, D sunk in turn, usually by a ULN2003; 2048 full steps (4096 half "
                       "steps) per turn of the shaft, whose angle is shown. Behavioral: no back EMF or inertia",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-3, -6],
        "value_offset": [-3, 5],
        "parameters": [
            number("R", "Coil resistance", "Ohm", "50", "Resistance of each coil half", "1", "1k"),
            number("L", "Coil inductance", "H", "20m", "Inductance of each coil half", "10u", "10"),
        ],
        "readout": {"kind": "node", "element": "ang_{ref}", "scale": 1, "unit": "°"},
        # 8 electrical cycles per motor turn, 64 motor turns per shaft turn.
        "spice": spice + rotor("20000", "512", ia, ib),
    }


def encoder():
    graphics = [{"circle": [0, 0, 2.4]}, {"circle": [0, 0, 0.8]},
                {"line": [0, -0.8, 0, -2.2]},
                {"line": [-5, -2, -2, -2]}, {"line": [-5, 0, -2.4, 0]}, {"line": [-5, 2, -2, 2]},
                {"text": "A", "at": [-4, -2.6], "size": 0.5}, {"text": "C", "at": [-4, -0.6], "size": 0.5},
                {"text": "B", "at": [-4, 1.4], "size": 0.5}]
    pins = [pin("A", -5, -2, "left"), pin("C", -5, 0, "left"), pin("B", -5, 2, "left")]
    d = "(20*V({control}))"
    frac = f"({d}-floor({d}))"
    closed_a = f"u({frac}-0.2)*u(0.6-{frac})"
    closed_b = f"u({frac}-0.4)*u(0.8-{frac})"
    return {
        "type": "encoder",
        "name": "Rotary encoder",
        "category": CATEGORY,
        "prefix": "SW",
        "description": "Incremental rotary encoder like an EC11 or KY-040: contacts A and B close to C in "
                       "quadrature, one cycle per detent (A before B turning clockwise), both open at rest; 20 "
                       "detents per turn. Turn it while running by dragging it or with the wheel (one detent per "
                       "step); pull-ups on A and B (or the board's) give the pulses. For the push switch place a push "
                       "button",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [3, -3],
        "value_offset": [3, 3],
        "value_parameter": "position",
        "parameters": [
            number("position", "Position", "", "10", "Detent the knob starts at (0 to 20, one turn); drag or wheel "
                   "while running", "0", "20"),
        ],
        "interactive": {"kind": "potentiometer", "parameter": "position"},
        "readout": {"kind": "node", "element": "pos_{ref}", "scale": 1, "unit": ""},
        "spice": [
            f"B_{{ref}}_pos pos_{{ref}} 0 V={d}",
            f"B_{{ref}}_a {{node:A}} {{node:C}} I=V({{node:A}},{{node:C}})*({closed_a}*1+1e-9)",
            f"B_{{ref}}_b {{node:B}} {{node:C}} I=V({{node:B}},{{node:C}})*({closed_b}*1+1e-9)",
        ],
    }


PARTS = [l298, uln2003, stepper_bipolar, stepper_28byj48, encoder]


def main():
    os.makedirs(FOLDER, exist_ok=True)
    for make in PARTS:
        p = make()
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            json.dump(p, fh, indent=2, ensure_ascii=False)
            fh.write("\n")
    print(f"Wrote {len(PARTS)} parts to {FOLDER}")


if __name__ == "__main__":
    main()
