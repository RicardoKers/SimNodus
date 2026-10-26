"""Writes the power and interface parts of resources/library/power_interface (SN-227).

- LM2596 buck regulator, as the chip (FB pin, the datasheet's inductor and
  diode outside) and as the common adjustable module (IN, OUT, the trimmer
  turned while running). Averaged models: the output follows the regulation
  loop and the input current is the output power over 85 %; the 150 kHz
  switching and its ripple are left out.
- TL494 PWM controller (DIP-16): oscillator f = 1.1 / (RT CT) on the CT pin,
  dead-time and PWM comparators, two error amplifiers OR'ed on FEEDBACK, the
  5 V reference, single-ended or push-pull outputs (OUTPUT CTRL). Its
  oscillator needs time steps well below its period: transient mode, or a
  low frequency in interactive mode.
- MAX485 RS-485 transceiver (DIP-8) and MAX232 RS-232 transceiver (DIP-16),
  behavioral; the MAX232's charge pump is not simulated (its rails are
  internal), but its capacitors belong on a real board.

Run from the repository root: python tools/generate_interface_parts.py
"""
import json
import os

FOLDER = "resources/library/power_interface"
CATEGORY = "Power & interface"


def number(name, label, unit, default, description, minimum=None, maximum=None):
    p = {"name": name, "label": label, "unit": unit, "default": default, "description": description}
    if minimum is not None:
        p["min"] = minimum
    if maximum is not None:
        p["max"] = maximum
    return p


def dip(title, pins, optional=()):
    """Package view, pin 1 at the top left (as tools/generate_logic_chip_parts.py)."""
    n = len(pins)
    half = n // 2
    top = -half
    graphics = [{"rect": [-4, top, 8, 2 * half]}, {"arc": [0, top, 0.7, 180, 180]},
                {"text": title, "at": [0, 0], "size": 0.8}]
    result = []
    for i, name in enumerate(pins):
        if i < half:
            x_tip, x_edge, y, direction = -6, -4, top + 1 + 2 * i, "left"
        else:
            x_tip, x_edge, y, direction = 6, 4, top + 1 + 2 * (n - 1 - i), "right"
        graphics.append({"line": [x_tip, y, x_edge, y]})
        graphics.append({"text": name, "at": [x_edge + (1.4 if x_edge < 0 else -1.4), y], "size": 0.5})
        graphics.append({"text": str(i + 1), "at": [(x_tip + x_edge) / 2, y - 0.5], "size": 0.45})
        p = {"name": name, "at": [x_tip, y], "dir": direction}
        if name in optional:
            p["optional"] = True
        result.append(p)
    return {"graphics": graphics, "pins": result}, top


def v(a, b="GND"):
    return f"V({{node:{a}}},{{node:{b}}})"


def buck(vset, enable="1", vin="VIN", out="OUT", gnd="GND"):
    """Averaged buck: a regulated current into OUT; the input pays the output power over 85 %."""
    return [
        # c: output current the loop asks for, limited by the dropout (VIN - 1.5 V) and 3.5 A.
        f"B_{{ref}}_c 0 c_{{ref}} I=1m*({enable}*min(max(min(500*({vset}),500*({v(vin, out)}-1.5)),0),3.5)-V(c_{{ref}}))",
        "C_{ref}_c c_{ref} 0 100n",
        f"B_{{ref}}_o {{node:{gnd}}} {{node:{out}}} I=max(V(c_{{ref}}),0)",
        f"B_{{ref}}_i {{node:{vin}}} {{node:{gnd}}} I=max(V(c_{{ref}}),0)*max({v(out, gnd)},0)/(0.85*max({v(vin, gnd)},1))"
        f"+5m*({enable})",
        f"R_{{ref}}_l {{node:{out}}} {{node:{gnd}}} 100k",
    ]


def lm2596():
    graphics = [{"rect": [-4, -4, 8, 8]}, {"text": "LM2596", "at": [0, 0], "size": 0.75}]
    pins = []
    for name, x, y, direction in (("VIN", -6, -2, "left"), ("ON", -6, 2, "left"), ("OUT", 6, -2, "right"),
                                  ("FB", 6, 2, "right"), ("GND", 0, 6, "down")):
        pins.append({"name": name, "at": [x, y], "dir": direction})
        if direction == "down":
            graphics.append({"line": [x, y, x, 4]})
        else:
            graphics.append({"line": [x, y, x + (2 if x < 0 else -2), y]})
        graphics.append({"text": "ON" if name == "ON" else name,
                         "at": [x + (3.2 if x < 0 else -3.2) if direction != "down" else 0, y if direction != "down" else 3.2],
                         "size": 0.5})
    graphics.append({"line": [-3.6, 1.4, -2.8, 1.4]})  # the bar over ON: active low
    enable = f"(1-0.5*(1+tanh(10*({v('ON')}-1.3))))"
    return {
        "type": "lm2596",
        "name": "Buck regulator LM2596",
        "category": CATEGORY,
        "prefix": "U",
        "description": "Step-down switching regulator (LM2596-ADJ), averaged: OUT regulates FB to 1.23 V (a divider "
                       "sets the output: Vout = 1.23 x (1 + R2/R1)), up to 3 A and about 1.5 V below VIN; the input "
                       "current is the output power over 85 % efficiency. Wire the datasheet's inductor (33 uH) and "
                       "Schottky diode, or not: the 150 kHz switching is not simulated. ON low (or GND) turns it on",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-4, -6],
        "value_offset": [5, 6],
        "spice": buck(f"1.23-{v('FB')}", enable) + ["R_{ref}_fb {node:FB} {node:GND} 100Meg",
                                                     "R_{ref}_on {node:ON} {node:GND} 100Meg"],
    }


def lm2596_module():
    graphics = [{"rect": [-6, -3, 12, 6]}, {"text": "LM2596", "at": [0, -0.8], "size": 0.7},
                {"text": "DC-DC", "at": [0, 1.0], "size": 0.55},
                {"line": [-8, -2, -6, -2]}, {"line": [-8, 2, -6, 2]}, {"line": [8, -2, 6, -2]}, {"line": [8, 2, 6, 2]},
                {"text": "IN+", "at": [-4.6, -2], "size": 0.45}, {"text": "IN-", "at": [-4.6, 2], "size": 0.45},
                {"text": "OUT+", "at": [4.3, -2], "size": 0.45}, {"text": "OUT-", "at": [4.3, 2], "size": 0.45},
                {"circle": [3.5, 0.6, 0.6]}, {"line": [3.1, 1.0, 3.9, 0.2]}]
    pins = [{"name": "IN+", "at": [-8, -2], "dir": "left"}, {"name": "IN-", "at": [-8, 2], "dir": "left"},
            {"name": "OUT+", "at": [8, -2], "dir": "right"}, {"name": "OUT-", "at": [8, 2], "dir": "right"}]
    spice = ["B_{ref}_vs vs_{ref} 0 V=1.25+33.75*V({control})", "R_{ref}_ret {node:OUT-} {node:IN-} 0.01"]
    spice += buck(f"V(vs_{{ref}})-{v('OUT+', 'IN-')}", vin="IN+", out="OUT+", gnd="IN-")
    return {
        "type": "lm2596_module",
        "name": "Buck converter module LM2596",
        "category": CATEGORY,
        "prefix": "U",
        "description": "The common adjustable LM2596 step-down module: input 4.5-40 V on IN+/IN-, output from 1.25 V up "
                       "to about 1.5 V below the input on OUT+/OUT- (OUT- is IN-), up to 3 A; turn its trimmer while "
                       "running by dragging it or with the wheel. Averaged: no switching ripple; the input current is "
                       "the output power over 85 %",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-6, -5],
        "value_offset": [-6, 5],
        "value_parameter": "vout",
        "parameters": [number("vout", "Output voltage", "V", "5", "Where the trimmer starts; drag the module or use "
                              "the wheel while running (1.25 to 35 V)", "1.25", "35")],
        "interactive": {"kind": "potentiometer", "parameter": "vout"},
        "readout": {"kind": "voltage", "pins": ["OUT+", "OUT-"]},
        "spice": spice,
    }


def tl494():
    pins = ["1IN+", "1IN-", "FEEDBACK", "DTC", "CT", "RT", "GND", "C1", "E1", "E2", "C2", "VCC", "OUTCTRL", "REF",
            "2IN-", "2IN+"]
    symbol, top = dip("TL494", pins, optional=("1IN+", "1IN-", "2IN+", "2IN-", "C1", "E1", "C2", "E2", "OUTCTRL"))
    s = f"0.5*(1+tanh(5*({v('VCC')}-6.5)))"
    level = lambda expr: f"0.5*(1+tanh(50*({expr})))"
    spice = [
        f"B_{{ref}}_s s_{{ref}} 0 V={s}",
        # 5 V reference and the RT pin at 3.65 V; the CT charging current mirrors RT's.
        f"B_{{ref}}_r r_{{ref}} {{node:GND}} V=5*V(s_{{ref}})",
        "R_{ref}_r r_{ref} {node:REF} 10",
        f"B_{{ref}}_rt rt_{{ref}} {{node:GND}} V=3.65*V(s_{{ref}})",
        "V_{ref}_rt rt_{ref} {node:RT} DC 0",
        # Oscillator: charge CT to 3 V, discharge it fast to 0.1 V (st = 1 while discharging).
        f"B_{{ref}}_st 0 st_{{ref}} I=1m*({level(v('CT') + '-0.1-2.9*(1-V(dis_' + '{ref}))')}-V(st_{{ref}}))",
        "C_{ref}_st st_{ref} 0 1n",
        # The discharge follows the decided state (above 0.5), so it cannot hold CT at the threshold.
        "B_{ref}_dis dis_{ref} 0 V=0.5*(1+tanh(100*(V(st_{ref})-0.5)))",
        f"B_{{ref}}_ct {{node:GND}} {{node:CT}} I=(1-V(dis_{{ref}}))*0.904*max(I(V_{{ref}}_rt),0)-V(dis_{{ref}})*{v('CT')}/20",
        # Error amplifiers OR'ed on FEEDBACK through 1 kOhm (a compensation network can pull it).
        f"B_{{ref}}_e1 e1_{{ref}} 0 V=V(s_{{ref}})*max(min(2000*({v('1IN+')}-{v('1IN-')}),3.5),0)",
        f"B_{{ref}}_e2 e2_{{ref}} 0 V=V(s_{{ref}})*max(min(2000*({v('2IN+')}-{v('2IN-')}),3.5),0)",
        "B_{ref}_e e_{ref} {node:GND} V=max(V(e1_{ref}),V(e2_{ref}))",
        "R_{ref}_e e_{ref} {node:FEEDBACK} 1k",
        # Outputs on while the ramp is above FEEDBACK and DTC + 0.12 V, not while discharging.
        f"B_{{ref}}_on on_{{ref}} 0 V=V(s_{{ref}})*(1-V(dis_{{ref}}))*{level(v('CT') + '-' + v('FEEDBACK'))}"
        f"*{level(v('CT') + '-' + v('DTC') + '-0.12')}",
        # Push-pull steering: a toggle flip-flop clocked by the discharge.
        "B_{ref}_m 0 m_{ref} I=1m*(V(dis_{ref})*((1-V(t_{ref}))-V(m_{ref})))",
        "C_{ref}_m m_{ref} 0 1n",
        "B_{ref}_t 0 t_{ref} I=1m*((1-V(dis_{ref}))*(V(m_{ref})-V(t_{ref})))",
        "C_{ref}_t t_{ref} 0 1n",
        f"B_{{ref}}_pp pp_{{ref}} 0 V={level(v('OUTCTRL') + '-2.5')}",
        "B_{ref}_o1 o1_{ref} 0 V=V(on_{ref})*(1-V(pp_{ref})+V(pp_{ref})*V(t_{ref}))",
        "B_{ref}_o2 o2_{ref} 0 V=V(on_{ref})*(1-V(pp_{ref})+V(pp_{ref})*(1-V(t_{ref})))",
        f"B_{{ref}}_q1 {{node:C1}} {{node:E1}} I=V(o1_{{ref}})*max({v('C1', 'E1')},0)/5",
        f"B_{{ref}}_q2 {{node:C2}} {{node:E2}} I=V(o2_{{ref}})*max({v('C2', 'E2')},0)/5",
        "R_{ref}_ctl {node:CT} {node:GND} 100Meg",  # a DC path for the timing capacitor
        # Leakage to GND: the output pins may stay open.
        "R_{ref}_c1 {node:C1} {node:GND} 100Meg",
        "R_{ref}_e1 {node:E1} {node:GND} 100Meg",
        "R_{ref}_c2 {node:C2} {node:GND} 100Meg",
        "R_{ref}_e2 {node:E2} {node:GND} 100Meg",
        "R_{ref}_q {node:VCC} {node:GND} 1k",
    ]
    spice += [f"R_{{ref}}_i{i} {{node:{p}}} {{node:GND}} 1G" for i, p in
              enumerate(["1IN+", "1IN-", "2IN+", "2IN-", "DTC", "OUTCTRL"])]
    return {
        "type": "tl494",
        "name": "PWM controller TL494",
        "category": CATEGORY,
        "prefix": "U",
        "description": "Pulse-width modulation controller (TL494, DIP-16): the oscillator runs at 1.1 / (RT x CT) with "
                       "a 0-3 V sawtooth on CT; the output transistors (C1-E1, C2-E2) conduct while the sawtooth is "
                       "above FEEDBACK (the higher of the two error amplifiers) and above DTC + 0.12 V; OUTPUT CTRL "
                       "to GND drives both together, to REF (5 V) alternately (push-pull). Supply 7-40 V. Behavioral; "
                       "its oscillator needs time steps well below its period (transient mode, or a low frequency)",
        "symbol": symbol,
        "designator_offset": [-1, top - 2],
        "value_offset": [-1, -top + 2],
        "spice": spice,
    }


def driver(tag, value, pin, enable, supply="VCC", ground="GND", r=30):
    return [
        f"B_{{ref}}_x{tag} x{tag}_{{ref}} 0 V={value}",
        f"B_{{ref}}_u{tag} {{node:{supply}}} {{node:{pin}}} I={enable}*V(x{tag}_{{ref}})*{v(supply, pin)}/{r}",
        f"B_{{ref}}_d{tag} {{node:{pin}}} {{node:{ground}}} I={enable}*(1-V(x{tag}_{{ref}}))*{v(pin, ground)}/{r}",
    ]


def max485():
    pins = ["RO", "RE", "DE", "DI", "GND", "A", "B", "VCC"]
    symbol, top = dip("MAX485", pins, optional=("RO", "RE", "DE", "DI"))
    ttl = lambda p: f"0.5*(1+tanh(10*({v(p)}-1.4)))"
    de = ttl("DE")
    spice = driver("a", ttl("DI"), "A", de) + driver("b", f"(1-{ttl('DI')})", "B", de)
    spice += driver("ro", f"0.5*(1+tanh(50*({v('A', 'B')})))", "RO", f"(1-{ttl('RE')})")
    spice += ["R_{ref}_ia {node:A} {node:GND} 12k", "R_{ref}_ib {node:B} {node:GND} 12k",
              "R_{ref}_q {node:VCC} {node:GND} 10k"]
    spice += [f"R_{{ref}}_i{p.lower()} {{node:{p}}} {{node:GND}} 1G" for p in ("RE", "DE", "DI")]
    return {
        "type": "max485",
        "name": "RS-485 transceiver MAX485",
        "category": CATEGORY,
        "prefix": "U",
        "description": "RS-485 half-duplex transceiver (MAX485, DIP-8): with DE high the driver puts DI on A and its "
                       "complement on B; with RE low RO is high while A is above B. Tie DE and RE together for a "
                       "direction pin; terminate long lines with 120 ohm between A and B. Supply 5 V. Behavioral",
        "symbol": symbol,
        "designator_offset": [-1, top - 2],
        "value_offset": [-1, -top + 2],
        "spice": spice,
    }


def max232():
    pins = ["C1+", "V+", "C1-", "C2+", "C2-", "V-", "T2OUT", "R2IN", "R2OUT", "T2IN", "T1IN", "R1OUT", "R1IN",
            "T1OUT", "GND", "VCC"]
    symbol, top = dip("MAX232", pins, optional=("C1+", "V+", "C1-", "C2+", "C2-", "V-", "T2OUT", "R2IN", "R2OUT",
                                                 "T2IN", "T1IN", "R1OUT", "R1IN", "T1OUT"))
    rail = f"max(2*{v('VCC')}-1.5,0)"
    ttl = lambda p: f"0.5*(1+tanh(10*({v(p)}-1.4)))"
    spice = [
        f"B_{{ref}}_vp vp_{{ref}} {{node:GND}} V={rail}",
        f"B_{{ref}}_vn vn_{{ref}} {{node:GND}} V=-{rail}",
        "R_{ref}_vp vp_{ref} {node:V+} 300",
        "R_{ref}_vn vn_{ref} {node:V-} 300",
    ]
    for k in (1, 2):
        # Transmitters invert: a low TTL input gives V+, a high one V-.
        spice += [
            f"B_{{ref}}_t{k} t{k}_{{ref}} {{node:GND}} V=(1-2*{ttl(f'T{k}IN')})*{rail}",
            f"R_{{ref}}_t{k} t{k}_{{ref}} {{node:T{k}OUT}} 300",
            f"R_{{ref}}_ti{k} {{node:T{k}IN}} {{node:GND}} 1G",
            # Receivers invert too: above about 1.5 V the TTL output is low.
            f"R_{{ref}}_ri{k} {{node:R{k}IN}} {{node:GND}} 5k",
            f"B_{{ref}}_r{k} r{k}_{{ref}} {{node:GND}} V={v('VCC')}*(1-0.5*(1+tanh(10*({v(f'R{k}IN')}-1.5))))",
            f"R_{{ref}}_r{k} r{k}_{{ref}} {{node:R{k}OUT}} 50",
        ]
    spice += ["R_{ref}_q {node:VCC} {node:GND} 1k"]
    return {
        "type": "max232",
        "name": "RS-232 transceiver MAX232",
        "category": CATEGORY,
        "prefix": "U",
        "description": "RS-232 line driver and receiver (MAX232, DIP-16) on 5 V: T1IN/T2IN (TTL) give inverted levels "
                       "of about +-8.5 V on T1OUT/T2OUT; R1IN/R2IN (RS-232, 5 kohm) give inverted TTL levels on "
                       "R1OUT/R2OUT. The charge pump is not simulated (its rails are inside), but a real board needs "
                       "its four 1 uF capacitors on C1+/C1-, C2+/C2-, V+ and V-. Behavioral",
        "symbol": symbol,
        "designator_offset": [-1, top - 2],
        "value_offset": [-1, -top + 2],
        "spice": spice,
    }


PARTS = [lm2596, lm2596_module, tl494, max485, max232]


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
