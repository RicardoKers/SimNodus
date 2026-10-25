"""Writes the 555 timer of resources/library/ics (SN-213).

A behavioral NE555/LM555 with the chip's pins (1 GND, 2 TRIG, 3 OUT, 4 RESET,
5 CTRL, 6 THR, 7 DIS, 8 VCC):

- Three 5 kOhm resistors from VCC to GND set CTRL at 2/3 VCC and the lower
  threshold at 1/3 VCC (half of CTRL when CTRL is driven).
- TRIG below the lower threshold sets the flip-flop (OUT high, discharge off);
  THR above CTRL resets it (OUT low, DIS to GND); TRIG wins when both hold, as
  on the chip; RESET below 0.7 V resets it. The flip-flop is master-slave on
  1 pF capacitors, like the flip-flops of ADR 0099 (interactive runs carry
  their voltages across segments): the master latches with positive feedback,
  and the slave, which drives OUT and DIS, copies it only once it is clearly 0
  or 1. With one latch driving DIS directly, ngspice's large steps settled on
  the oscillator's unstable middle point (OUT halfway, the capacitor still).
- OUT is VCC minus the output drop when high, 0 V when low, behind 10 Ohm
  (the bipolar chip sources and sinks about 200 mA); DIS is about 10 Ohm to
  GND while OUT is low.
- RESET has a 100 kOhm pull-up to VCC so it may stay open (the chip needs it
  wired); CTRL may stay open.
- About 3 mA of supply current; TRIG and THR draw no current.

Run from the repository root: python tools/generate_timer_parts.py
"""
import json
import os

FOLDER = "resources/library/ics"


def step(expression):
    """0..1, switching within a few mV of 0."""
    return f"0.5*(1+tanh(({expression})/0.005))"


def timer_555():
    def v(pin):
        return f"V({{node:{pin}}},{{node:GND}})"

    # Every decision waits for the supply: each segment of an interactive run
    # starts from 0 V on the nodes without a capacitor, and a 0 V RESET there
    # would clear the flip-flop that its capacitor carried.
    powered = step(f"{v('VCC')}-3")
    trig = f"{powered}*{step('V(lo_{ref},{node:GND})-' + v('TRIG'))}"
    thr = f"{powered}*{step(v('THR') + '-' + v('CTRL'))}"
    reset = f"{powered}*(1-{step(v('RESET') + '-0.7')})"
    master = "V(m_{ref})"
    slave = "V(s_{ref})"
    spice = [
        "R_{ref}_1 {node:VCC} {node:CTRL} 5k",
        "R_{ref}_2 {node:CTRL} lo_{ref} 5k",
        "R_{ref}_3 lo_{ref} {node:GND} 5k",
        # Master: set by TRIG, reset by THR (TRIG wins) or RESET; otherwise it
        # regenerates toward 0 or 1.
        f"B_{{ref}}_m 0 m_{{ref}} I=1m*({trig}*(1-{reset})*(1-{master})"
        f"-(({thr})*(1-{trig})+{reset})*{master}"
        f"+(1-{trig})*(1-{thr})*(1-{reset})*({step(master + '-0.5')}-{master}))",
        "C_{ref}_m m_{ref} 0 1p",
        # Slave: copies the master only once it is clearly 0 or 1.
        f"B_{{ref}}_s 0 s_{{ref}} I=1m*({step(master + '-0.9')}*(1-{slave})-{step('0.1-' + master)}*{slave})",
        "C_{ref}_s s_{ref} 0 1p",
        f"B_{{ref}}_o o_{{ref}} {{node:GND}} V={step(slave + '-0.5')}*max({v('VCC')}-{{Vdrop}},0)",
        "R_{ref}_o o_{ref} {node:OUT} 10",
        f"B_{{ref}}_d {{node:DIS}} {{node:GND}} I={v('DIS')}*(1-{step(slave + '-0.5')})/10",
        "R_{ref}_r {node:VCC} {node:RESET} 100k",
        "R_{ref}_q {node:VCC} {node:GND} 2k",
        "R_{ref}_t {node:TRIG} {node:GND} 1G",
        "R_{ref}_h {node:THR} {node:GND} 1G",
        "R_{ref}_x {node:DIS} {node:GND} 1G",
    ]
    graphics = [{"rect": [-4, -5, 8, 10]}, {"text": "555", "at": [0, -3.8], "size": 0.9}]
    pins = []
    layout = [  # name, chip pin, at, direction, label position
        ("TRIG", 2, [-6, -2], "left", [-2.6, -2]),
        ("THR", 6, [-6, 0], "left", [-2.8, 0]),
        ("DIS", 7, [-6, 2], "left", [-2.9, 2]),
        ("OUT", 3, [6, 0], "right", [2.8, 0]),
        ("VCC", 8, [-2, -7], "up", [-2, -4.2]),
        ("RESET", 4, [2, -7], "up", [2, -4.2]),
        ("GND", 1, [-2, 7], "down", [-2, 4.2]),
        ("CTRL", 5, [2, 7], "down", [2, 4.2]),
    ]
    for name, number, at, direction, label in layout:
        x, y = at
        edge = {"left": [-4, y], "right": [4, y], "up": [x, -5], "down": [x, 5]}[direction]
        graphics.append({"line": [x, y, edge[0], edge[1]]})
        graphics.append({"text": name, "at": label, "size": 0.6})
        # The chip's pin number beside the lead.
        offset = {"left": [-0.5, -0.5], "right": [0.5, -0.5], "up": [0.5, -0.8], "down": [0.5, 0.8]}[direction]
        graphics.append({"text": str(number), "at": [edge[0] + offset[0], edge[1] + offset[1]], "size": 0.5})
        pin = {"name": name, "at": at, "dir": direction}
        if name in ("RESET", "CTRL"):
            pin["optional"] = True
        pins.append(pin)
    return {
        "type": "timer_555",
        "name": "Timer 555",
        "category": "Integrated circuits",
        "prefix": "U",
        "description": "NE555/LM555 timer (behavioral): TRIG below 1/3 VCC sets OUT high and releases DIS, THR above "
                       "2/3 VCC (CTRL) sets OUT low and DIS to GND; RESET low forces OUT low. Astable and monostable "
                       "circuits as in the datasheet; supply 4.5-16 V",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [5, -6],
        "value_offset": [5, 6],
        "parameters": [
            {"name": "Vdrop", "label": "Output high drop", "unit": "V", "default": "1.7",
             "description": "OUT high is this far below VCC (1.7 V for the bipolar NE555; 0 for a CMOS 555)",
             "min": "0", "max": "3"},
        ],
        "spice": spice,
    }


def main():
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "part_descriptions.json"), encoding="utf-8") as fh:
        short = json.load(fh)
    p = timer_555()
    p["description"] = short.get(p["type"], p["description"])
    with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
