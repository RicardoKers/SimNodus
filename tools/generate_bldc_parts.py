"""Writes the BLDC motors and the three-phase bridge of resources/library/motion (E-11, SN-276/SN-277).

Motor: three windings (R + L) in star with an internal neutral; the back-EMF of
phase k is ke * w * f(p * theta - k * 120 deg), with f trapezoidal (120 deg
flat tops: the BLDC motor) or sinusoidal (the PMSM); the torque is
sum(ke * f_k * i_k), so the electrical power sum(e_k * i_k) equals torque x
speed. The rotor's speed w (rad/s) and angle are internal nodes, with an
inertia from the mechanical time constant, friction from the no-load current
and a live load torque (0-100 % of the stall torque, friction-like as in the
DC motor of SN-271).

ke is chosen so the motor reaches its no-load speed at its rated voltage:
six-step puts two windings in series across the bus, so for the trapezoidal
motor 2 ke w0 = V; sinusoidal commutation with space-vector (or third-harmonic)
PWM gives a line-to-line peak of V, so for the PMSM sqrt(3) ke w0 = V.

Hall sensors: three open-collector outputs (pull-ups needed) 120 deg apart,
switching at 30, 90, 150 ... electrical degrees, where six-step commutates.
Codes HA HB HC (1 = released, high through the pull-up) by sector of the
electrical angle, and the phases to drive for forward rotation:
    30-90: 101 U+ V-     90-150: 100 U+ W-    150-210: 110 V+ W-
    210-270: 010 V+ U-   270-330: 011 W+ U-   330-30: 001 W+ V-

Bridge: three half-bridges with IN and EN (DRV8313 style). Each output is
VM * d behind the on-resistance, d = V(IN) / logic level clamped to 0..1, and
EN connects it (Hi-Z when low, body diodes to VM and GND). With exact
microcontroller edges IN is 0 or the logic level and the bridge switches;
without them IN is the PWM's step average and the bridge gives the average
voltage (ADR 0091). VM supplies sum(d_k * i_k). EN is a conductance in
proportion to its level and the body diodes are straight segments: switches
and exponential diodes made ngspice take 6 us steps around every
commutation (SN-278).

Run from the repository root: python tools/generate_bldc_parts.py
"""
import json
import math
import os

FOLDER = "resources/library/motion"
CATEGORY = "Electromechanical"
TWO_PI = 2 * math.pi


def number(name, label, unit, default, description, minimum=None, maximum=None):
    p = {"name": name, "label": label, "unit": unit, "default": default, "description": description}
    if minimum is not None:
        p["min"] = minimum
    if maximum is not None:
        p["max"] = maximum
    return p


def pin(name, x, y, direction):
    return {"name": name, "at": [x, y], "dir": direction}


def motor_symbol():
    graphics = [
        {"circle": [0, 0, 3.2]},
        {"text": "M", "at": [0, -0.6], "size": 1.8},
        {"text": "3~", "at": [0, 1.4], "size": 1.0},
        {"rect": [5, -4.6, 4, 6.2]},
        {"text": "Hall", "at": [7, -3.6], "size": 0.8},
        {"line": [3.2, 0, 5, 0]},
    ]
    pins = []
    for name, y in (("U", -2), ("V", 0), ("W", 2)):
        graphics.append({"line": [-6, y, -3.0 if y == 0 else -2.5, y]})
        graphics.append({"text": name, "at": [-5, y - 0.6], "size": 0.8})
        pins.append(pin(name, -6, y, "left"))
    for name, y in (("HA", -2), ("HB", -1), ("HC", 0), ("GND", 1)):
        graphics.append({"line": [9, y, 11, y]})
        graphics.append({"text": name, "at": [7.6, y], "size": 0.65})
        pins.append(pin(name, 11, y, "right"))
    return {"graphics": graphics, "pins": pins}


def motor(kind, name, sinusoidal):
    ke = "({= V / (1.7320508 * rpm * 6.2831853 / 60)})" if sinusoidal else "({= V / (2 * rpm * 6.2831853 / 60)})"
    # Effective torque constant and resistance of the motor as a DC machine
    # (two windings under six-step, or the equivalent for sine commutation).
    kt = "(2 * V / (2 * rpm * 6.2831853 / 60))" if not sinusoidal else "(1.5 * V / (1.7320508 * rpm * 6.2831853 / 60))"
    w0 = "(rpm * 6.2831853 / 60)"
    if sinusoidal:
        f = "sin({x})"
    else:
        # asin(sin(x)) is a triangle of slope 1; clamped at pi/6 it is a trapezoid
        # with 120 deg flat tops centered on 90 deg.
        f = "max(-1,min(1,asin(sin({x}))/0.52359878))"
    th = "{pairs}*V(th_{ref})"
    phases = {"U": f"{th}", "V": f"{th}-2.0943951", "W": f"{th}-4.1887902"}
    lines = []
    for p, x in phases.items():
        lines += [
            f"V_{{ref}}_s{p.lower()} {{node:{p}}} a{p.lower()}_{{ref}} DC 0",
            f"R_{{ref}}_{p.lower()} a{p.lower()}_{{ref}} b{p.lower()}_{{ref}} {{R}}",
            f"L_{{ref}}_{p.lower()} b{p.lower()}_{{ref}} e{p.lower()}_{{ref}} {{L}}",
            f"B_{{ref}}_f{p.lower()} f{p.lower()}_{{ref}} 0 V={f.format(x='(' + x + ')')}",
            f"B_{{ref}}_e{p.lower()} e{p.lower()}_{{ref}} n_{{ref}} V={ke}*V(w_{{ref}})*V(f{p.lower()}_{{ref}})",
        ]
    lines += [
        # Torque into the speed node: sum of ke * f_k * i_k.
        f"B_{{ref}}_t 0 w_{{ref}} I={ke}*(V(fu_{{ref}})*I(V_{{ref}}_su)+V(fv_{{ref}})*I(V_{{ref}}_sv)"
        f"+V(fw_{{ref}})*I(V_{{ref}}_sw))",
        # Inertia from the mechanical time constant (as a DC machine of 2R), friction from the no-load current.
        f"C_{{ref}}_j w_{{ref}} 0 {{= tm * {kt} * {kt} / (2 * R)}}",
        f"R_{{ref}}_b w_{{ref}} 0 {{= {w0} / ({kt} * Inoload)}}",
        # Load torque, a share of the stall torque kt * V / 2R; friction-like.
        f"B_{{ref}}_load w_{{ref}} 0 I={{= {kt} * V / (2 * R)}}*V({{control}})*tanh(V(w_{{ref}})/{{= {w0} / 100}})",
        # The rotor angle (mechanical radians) integrates the speed.
        "B_{ref}_th 0 th_{ref} I=V(w_{ref})",
        "C_{ref}_th th_{ref} 0 1",
        "R_{ref}_th th_{ref} 0 1T",
        "R_{ref}_n n_{ref} 0 1G",
    ]
    # Hall sensors: open collector, low (switch closed) while the sensor's bit is 0.
    for k, h in enumerate(("HA", "HB", "HC")):
        lines += [
            f"B_{{ref}}_{h.lower()} {h.lower()}_{{ref}} 0 V=-sin({th}-0.52359878-{k}*2.0943951)",
            f"S_{{ref}}_{h.lower()} {{node:{h}}} {{node:GND}} {h.lower()}_{{ref}} 0 SW_HALL",
        ]
    description = (
        ("Three-phase permanent-magnet synchronous motor (PMSM) with sinusoidal back-EMF, for sinusoidal "
         "commutation and FOC" if sinusoidal else
         "Three-phase brushless DC motor (BLDC) with trapezoidal back-EMF, for six-step commutation")
        + "; three open-collector Hall sensor outputs (add pull-ups); shows its speed; the load is dragged while "
          "running. Experimental (E-11)")
    return {
        "type": kind,
        "name": name,
        "category": CATEGORY,
        "prefix": "M",
        "description": description,
        "symbol": motor_symbol(),
        "designator_offset": [-3, -5],
        "value_offset": [-3, 5],
        "value_parameter": "V",
        "more_values": ["load"],
        "parameters": [
            number("V", "Rated voltage", "V", "12", "Bus voltage for the no-load speed", "1", "100"),
            number("rpm", "No-load speed", "rpm", "3000",
                   "Speed at the rated voltage without load, under " + ("sinusoidal commutation with space-vector PWM"
                                                                       if sinusoidal else "six-step commutation"),
                   "10", "100k"),
            number("R", "Phase resistance", "Ohm", "0.5", "Resistance of each winding (line to line: twice this)",
                   "1m", "100"),
            number("L", "Phase inductance", "H", "500u", "Inductance of each winding", "1u", "1"),
            number("pairs", "Pole pairs", "", "4", "Pole pairs: electrical turns per mechanical turn", "1", "20"),
            number("Inoload", "No-load current", "A", "200m", "Bus current at the no-load speed (sets the friction)",
                   "1m", "10"),
            number("tm", "Mechanical time constant", "s", "50m",
                   "Time to reach 63 % of the speed from rest with the shaft free; sets the inertia", "1m", "10"),
            number("load", "Load", "%", "0",
                   "Load torque on the shaft as a share of the stall torque; drag the part or use the wheel while running",
                   "0", "100"),
        ],
        "interactive": {"kind": "potentiometer", "parameter": "load"},
        "models": [".model SW_HALL SW(VT=0 VH=0.02 RON=10 ROFF=1e9)"],
        "readout": {"kind": "node", "element": "w_{ref}", "scale": 60 / TWO_PI, "unit": "rpm"},
        "live": [
            {"label": "I(U)", "kind": "current", "element": "V_{ref}_su", "unit": "A"},
            {"label": "I(V)", "kind": "current", "element": "V_{ref}_sv", "unit": "A"},
            {"label": "I(W)", "kind": "current", "element": "V_{ref}_sw", "unit": "A"},
            {"label": "Speed", "kind": "node", "element": "w_{ref}", "scale": 60 / TWO_PI, "unit": "rpm"},
            {"label": "Load", "kind": "node", "element": "ctl_{ref}", "scale": 100, "unit": "%"},
        ],
        "spice": lines,
    }


def bridge(csa=False):
    graphics = [{"rect": [-6, -9, 12, 18]}, {"text": "3x half", "at": [0, -6.6], "size": 0.9},
                {"text": "bridge", "at": [0, -5.4], "size": 0.9}]
    pins = []
    left = [("IN1", -6), ("IN2", -4), ("IN3", -2), ("EN1", 2), ("EN2", 4), ("EN3", 6)]
    for name, y in left:
        graphics.append({"line": [-8, y, -6, y]})
        graphics.append({"text": name, "at": [-4.3, y], "size": 0.75})
        pins.append(pin(name, -8, y, "left"))
    right = [("OUT1", -2), ("OUT2", 0), ("OUT3", 2)]
    if csa:
        right += [("SO1", 4), ("SO2", 6), ("SO3", 8)]
    for name, y in right:
        graphics.append({"line": [6, y, 8, y]})
        graphics.append({"text": name, "at": [4.0, y], "size": 0.75})
        pins.append(pin(name, 8, y, "right"))
        if name.startswith("SO"):
            pins[-1]["optional"] = True  # a controller often reads only two phases
    graphics += [{"line": [0, -11, 0, -9]}, {"text": "VM", "at": [0, -8.2], "size": 0.75},
                 {"line": [0, 9, 0, 11]}, {"text": "GND", "at": [0, 8.2], "size": 0.75}]
    pins += [pin("VM", 0, -11, "up"), pin("GND", 0, 11, "down")]
    lines = []
    supply = []
    for k in (1, 2, 3):
        lines += [
            f"B_{{ref}}_d{k} d{k}_{{ref}} 0 V=min(max(V({{node:IN{k}}},{{node:GND}})/{{vio}},0),1)",
            f"B_{{ref}}_o{k} o{k}_{{ref}} {{node:GND}} V=V({{node:VM}},{{node:GND}})*V(d{k}_{{ref}})",
            f"R_{{ref}}_r{k} o{k}_{{ref}} s{k}_{{ref}} {{Ron}}",
            f"V_{{ref}}_i{k} s{k}_{{ref}} p{k}_{{ref}} DC 0",
            f"B_{{ref}}_c{k} c{k}_{{ref}} 0 V=V({{node:EN{k}}},{{node:GND}})/{{vio}}",
            # EN as a conductance in proportion to its (step-averaged) level: a leg
            # enabled a fraction x of the time conducts x times as much on average,
            # and the smooth change keeps ngspice's steps long (SN-278).
            f"B_{{ref}}_e{k} p{k}_{{ref}} x{k}_{{ref}} I=V(p{k}_{{ref}},x{k}_{{ref}})*(1e-9+1000*min(max(V(c{k}_{{ref}}),0),1))",
            # Body diodes as straight segments (0.6 V, 0.1 ohm): no exponential knee.
            f"B_{{ref}}_h{k} x{k}_{{ref}} {{node:VM}} I=10*max(V(x{k}_{{ref}},{{node:VM}})-0.6,0)",
            f"B_{{ref}}_l{k} {{node:GND}} x{k}_{{ref}} I=10*max(V({{node:GND}},x{k}_{{ref}})-0.6,0)",
            f"R_{{ref}}_z{k} x{k}_{{ref}} {{node:GND}} 10Meg",
            # The phase current, out of OUT into the load.
            f"V_{{ref}}_m{k} x{k}_{{ref}} {{node:OUT{k}}} DC 0",
        ]
        if csa:
            # Current-sense amplifier: offset + gain x phase current, clamped to
            # 0..logic level, behind 100 ohm (the phase current measured in line,
            # as the step average sees it).
            lines += [
                f"B_{{ref}}_a{k} a{k}_{{ref}} {{node:GND}} V=min(max({{offset}}+{{gain}}*I(V_{{ref}}_m{k}),0),{{vio}})",
                f"R_{{ref}}_a{k} a{k}_{{ref}} {{node:SO{k}}} 100",
            ]
        supply.append(f"V(d{k}_{{ref}})*I(V_{{ref}}_i{k})")
    lines.append(f"B_{{ref}}_vm {{node:VM}} {{node:GND}} I={'+'.join(supply)}")
    lines += [f"R_{{ref}}_in{k} {{node:IN{k}}} {{node:GND}} 1Meg" for k in (1, 2, 3)]
    lines += [f"R_{{ref}}_en{k} {{node:EN{k}}} {{node:GND}} 1Meg" for k in (1, 2, 3)]
    parameters = [
        number("vio", "Logic level", "V", "3.3", "Input high level (the microcontroller's supply)", "1.8", "5.5"),
        number("Ron", "On-resistance", "Ohm", "250m", "Resistance of each switch when on", "1m", "10"),
    ]
    description = ("Three half-bridges with IN and EN per phase (DRV8313 style): EN high connects OUT, to VM "
                   "while IN is high and to GND while it is low; EN low leaves OUT open (body diodes to VM and "
                   "GND). PWM goes on IN; with the microcontroller's step averages the output is the average "
                   "voltage. Experimental (E-11)")
    if csa:
        parameters += [
            number("gain", "Current-sense gain", "V/A", "0.2",
                   "SO volts per ampere of phase current (out of OUT positive)", "1m", "10"),
            number("offset", "Current-sense offset", "V", "1.65", "SO at zero current (half the logic level)",
                   "0", "5"),
        ]
        description = description.replace(
            ". Experimental", "; SO1-SO3 give each phase current as offset + gain x I (DRV8323 style, "
            "in-line sensing). Experimental")
    return {
        "type": "bridge_3phase_csa" if csa else "bridge_3phase",
        "name": "Three-phase half-bridge driver with current sense" if csa else "Three-phase half-bridge driver",
        "category": CATEGORY,
        "prefix": "U",
        "description": description,
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [7, -9],
        "parameters": parameters,
        "spice": lines,
    }


def main():
    for definition in (motor("bldc_motor", "BLDC motor", False), motor("pmsm_motor", "PMSM motor (sinusoidal)", True),
                       bridge(), bridge(csa=True)):
        path = os.path.join(FOLDER, definition["type"] + ".json")
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(definition, fh, indent=2, ensure_ascii=False)
            fh.write("\n")
        print(path)


if __name__ == "__main__":
    main()
