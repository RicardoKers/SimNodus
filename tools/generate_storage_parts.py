"""Writes the behavioral latches, flip-flops, counter and shift register of
resources/library/storage (SN-199, ADR 0099).

Each stored bit is the voltage of a 1 pF capacitor that a behavioral current
source charges toward its input while the part is enabled (a 1 ns time
constant) and leaves alone otherwise. Edge-triggered parts are master-slave:
the master follows its input while the clock is below 0.4 VDD, the slave
copies the master while the clock is above 0.6 VDD, so a rising edge moves the
input to the output once. Interactive runs carry capacitor voltages across
segments, so the state survives them without special handling. Outputs drive
like the logic gates: about 10 ns of delay and 50 ohm to 0 or VDD.

Run from the repository root: python tools/generate_storage_parts.py
"""
import json
import os

FOLDER = "resources/library/storage"
CATEGORY = "Flip-flops & counters"


def level(node):
    """0..1: the node's voltage against the VDD/2 threshold (as in the gates)."""
    return f"0.5*(1+tanh({{= 40 / VDD}}*(V({node})-{{= VDD / 2}})))"


def above(node, fraction):
    return f"0.5*(1+tanh({{= 40 / VDD}}*(V({node})-{{= {fraction} * VDD}})))"


def pin_node(name):
    return f"{{node:{name}}}"


VDD_PARAMETER = {
    "name": "VDD", "label": "Supply", "unit": "V", "default": "5",
    "description": "Logic supply: 5 V for 74HC, 3.3 V next to a Blue Pill; output high level and input threshold (VDD/2)",
    "min": "1.5", "max": "15",
}


def stored_bit(index, enable, target, clear=None):
    """A capacitor that follows `target` (0..1) while `enable` (0..1); `clear` forces 0."""
    node = f"{index}_{{ref}}"
    hold = f"(1-{clear})*" if clear else ""
    reset = f"-{clear}*V({node})" if clear else ""
    return [
        f"B_{{ref}}_{index} 0 {node} I=1m*({hold}{enable}*({{VDD}}*{target}-V({node})){reset})",
        f"C_{{ref}}_{index} {node} 0 1p",
    ]


def master_slave(bit, target, clock, clear=None):
    """Rising-edge flip-flop: master m<bit>, slave s<bit> (the stored output)."""
    low = f"(1-{above(clock, 0.4)})"
    high = above(clock, 0.6)
    lines = stored_bit(f"m{bit}", low, target, clear)
    # The slave copies the master's voltage: its target is V(m)/VDD.
    slave = f"s{bit}"
    node = f"{slave}_{{ref}}"
    hold = f"(1-{clear})*" if clear else ""
    reset = f"-{clear}*V({node})" if clear else ""
    lines += [
        f"B_{{ref}}_{slave} 0 {node} I=1m*({hold}{high}*(V(m{bit}_{{ref}})-V({node})){reset})",
        f"C_{{ref}}_{slave} {node} 0 1p",
    ]
    return lines


def output(state, pin, inverted=False, tag=None):
    """Drives `pin` from a state node, like a gate output."""
    tag = tag or pin.lower()
    value = f"(1-{level(state)})" if inverted else level(state)
    return [
        f"B_{{ref}}_x{tag} x{tag}_{{ref}} 0 V={{VDD}}*{value}",
        f"R_{{ref}}_d{tag} x{tag}_{{ref}} y{tag}_{{ref}} 1k",
        f"C_{{ref}}_d{tag} y{tag}_{{ref}} 0 10p",
        f"B_{{ref}}_o{tag} z{tag}_{{ref}} 0 V=V(y{tag}_{{ref}})",
        f"R_{{ref}}_o{tag} z{tag}_{{ref}} {pin_node(pin)} 50",
    ]


def inputs(pins):
    return [f"R_{{ref}}_i{p.lower()} {pin_node(p)} 0 1G" for p in pins]


def symbol(title, left, right, clock=None, optional=()):
    """Rectangle with inputs on the left, outputs on the right, two grid units apart."""
    rows = max(len(left), len(right))
    height = 2 * rows + 2
    top = -(height // 2)
    graphics = [{"rect": [-3, top, 6, height]}, {"text": title, "at": [0, top - 0.8], "size": 0.8}]
    pins = []

    def place(names, x_tip, x_edge, direction):
        first = -(len(names) - 1)
        for i, name in enumerate(names):
            y = first + 2 * i
            graphics.append({"line": [x_tip, y, x_edge, y]})
            label_x = x_edge + (0.9 if x_edge < 0 else -0.9)
            shown = name[:-1] if name.endswith("N") and name != "EN" else name
            if name == clock:
                graphics.append({"polyline": [-3, y - 0.6, -2.2, y, -3, y + 0.6]})
                label_x += 0.6
            graphics.append({"text": shown, "at": [label_x, y], "size": 0.8})
            if shown != name:  # an inverted output: a bar over its name
                graphics.append({"line": [label_x - 0.35, y - 0.6, label_x + 0.35, y - 0.6]})
            pin = {"name": name, "at": [x_tip, y], "dir": direction}
            if name in optional:
                pin["optional"] = True
            pins.append(pin)

    place(left, -5, -3, "left")
    place(right, 5, 3, "right")
    return {"graphics": graphics, "pins": pins}, top


def part(type_, name, description, left, right, spice, clock=None, optional=(), title=""):
    sym, top = symbol(title, left, right, clock, optional)
    return {
        "type": type_,
        "name": name,
        "category": CATEGORY,
        "prefix": "U",
        "description": description + "; behavioral, on a hidden supply VDD like the logic gates: inputs switch at "
                       "VDD/2, outputs go to 0 or VDD through 50 ohm with about 10 ns of delay; starts with all "
                       "outputs Q low",
        "symbol": sym,
        "designator_offset": [-3, top - 2],
        "value_offset": [-3, -top + 1],
        "value_parameter": "VDD",
        "parameters": [VDD_PARAMETER],
        "spice": spice + inputs(left),
    }


def q_outputs(state):
    return output(state, "Q") + output(state, "QN", inverted=True)


def build():
    parts = []

    # D latch: transparent while EN is high, holds while it is low.
    spice = stored_bit("s", level(pin_node("EN")), level(pin_node("D"))) + q_outputs("s_{ref}")
    parts.append(part("d_latch", "D latch", "D latch (like one bit of a 74HC75): Q follows D while EN is high and "
                      "keeps its value while EN is low", ["D", "EN"], ["Q", "QN"], spice, optional=("QN",),
                      title="D latch"))

    # SR latch: S sets, R resets; R wins when both are high.
    s, r = level(pin_node("S")), level(pin_node("R"))
    spice = [
        f"B_{{ref}}_s 0 s_{{ref}} I=1m*({s}*(1-{r})*({{VDD}}-V(s_{{ref}}))-{r}*V(s_{{ref}}))",
        "C_{ref}_s s_{ref} 0 1p",
    ] + q_outputs("s_{ref}")
    parts.append(part("sr_latch", "SR latch", "SR latch: a high S sets Q, a high R resets it (R wins when both are "
                      "high); with both low Q keeps its value", ["S", "R"], ["Q", "QN"], spice, optional=("QN",),
                      title="SR latch"))

    clock = pin_node("CLK")
    q = level("s0_{ref}")

    # D flip-flop: Q takes D at the rising clock edge.
    spice = master_slave(0, level(pin_node("D")), clock) + q_outputs("s0_{ref}")
    parts.append(part("d_flipflop", "D flip-flop", "D flip-flop (like one half of a 74HC74 without preset and "
                      "clear): Q takes the value of D at each rising edge of CLK", ["D", "CLK"], ["Q", "QN"], spice,
                      clock="CLK", optional=("QN",), title="D FF"))

    # JK flip-flop: J sets, K resets, both toggle, at the rising edge.
    j, k = level(pin_node("J")), level(pin_node("K"))
    spice = master_slave(0, f"({j}*(1-{q})+(1-{k})*{q})", clock) + q_outputs("s0_{ref}")
    parts.append(part("jk_flipflop", "JK flip-flop", "JK flip-flop, rising edge: J=1 K=0 sets Q, J=0 K=1 resets "
                      "it, J=K=1 toggles it, J=K=0 keeps it", ["J", "CLK", "K"], ["Q", "QN"], spice, clock="CLK",
                      optional=("QN",), title="JK FF"))

    # T flip-flop: toggles at the rising edge while T is high.
    t = level(pin_node("T"))
    spice = master_slave(0, f"({t}+{q}-2*{t}*{q})", clock) + q_outputs("s0_{ref}")
    parts.append(part("t_flipflop", "T flip-flop", "T flip-flop: Q changes at each rising edge of CLK while T is "
                      "high (a frequency divider by two with T high)", ["T", "CLK"], ["Q", "QN"], spice, clock="CLK",
                      optional=("QN",), title="T FF"))

    # 4-bit synchronous binary counter with asynchronous clear.
    clear = level(pin_node("CLR"))
    spice = []
    carry = "1"
    for bit in range(4):
        qb = level(f"s{bit}_{{ref}}")
        spice += master_slave(bit, f"({carry}+{qb}-2*{carry}*{qb})", clock, clear)
        carry = qb if bit == 0 else f"{carry}*{qb}"
    for bit in range(4):
        spice += output(f"s{bit}_{{ref}}", f"Q{bit}")
    parts.append(part("counter4", "4-bit counter", "Synchronous 4-bit binary up-counter (like a 74HC161 without "
                      "load and enable): counts 0..15 at the rising edges of CLK and starts again; a high CLR "
                      "resets it to 0 at once", ["CLK", "CLR"], ["Q0", "Q1", "Q2", "Q3"], spice, clock="CLK",
                      optional=("CLR",), title="CTR4"))

    # Decade counter: 0..9, then 0 again (from 9 = 1001 the next state is 0000).
    spice = []
    carry = "1"
    nine = f"({level('s0_{ref}')}*{level('s3_{ref}')})"
    for bit in range(4):
        qb = level(f"s{bit}_{{ref}}")
        spice += master_slave(bit, f"(1-{nine})*({carry}+{qb}-2*{carry}*{qb})", clock, clear)
        carry = qb if bit == 0 else f"{carry}*{qb}"
    for bit in range(4):
        spice += output(f"s{bit}_{{ref}}", f"Q{bit}")
    parts.append(part("counter10", "Decade counter", "Synchronous decade counter (like a 74HC160 without load and "
                      "enable): counts 0..9 in binary at the rising edges of CLK and starts again; a high CLR resets "
                      "it to 0 at once; drives a BCD to 7-segment decoder", ["CLK", "CLR"], ["Q0", "Q1", "Q2", "Q3"],
                      spice, clock="CLK", optional=("CLR",), title="CTR10"))

    # 4-bit shift register, serial in, parallel out, with asynchronous clear.
    spice = []
    for bit in range(4):
        target = level(pin_node("D")) if bit == 0 else level(f"s{bit - 1}_{{ref}}")
        spice += master_slave(bit, target, clock, clear)
    for bit in range(4):
        spice += output(f"s{bit}_{{ref}}", f"Q{bit}")
    parts.append(part("shift4", "4-bit shift register", "4-bit shift register, serial in and parallel out (like a "
                      "74HC164): at each rising edge of CLK, Q0 takes D and each other output takes the one before "
                      "it; a high CLR resets all outputs", ["D", "CLK", "CLR"], ["Q0", "Q1", "Q2", "Q3"], spice,
                      clock="CLK", optional=("CLR",), title="SRG4"))
    return parts


def main():
    os.makedirs(FOLDER, exist_ok=True)
    for p in build():
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
