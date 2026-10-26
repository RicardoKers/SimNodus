"""Writes the logic chips in DIP packages of resources/library/logic_chips (SN-222).

Real chips with every pin in its package position (pin 1 at the top left,
counting down the left side and up the right side, the notch on top) and a
supply: VCC and GND must be wired, as on a breadboard.

- Gates, decoder, multiplexer, bus transceiver and the dual D flip-flop are
  behavioral SPICE, so an RC oscillator with a 74HC14 runs at its real
  frequency. Inputs switch at VCC/2 (the 74HC14 at about 0.53 and 0.31 VCC);
  each output is a pair of 50 ohm switches to VCC and GND, so the load current
  comes from the VCC pin and an unpowered chip drives nothing; about 10 ns of
  delay.
- Counters and shift registers are digital devices (ADR 0102/0103): their
  logic runs in a device runner, with a board or on their own. Nets they share
  with a board or another chip are resolved at once; inputs from the rest of
  the circuit arrive up to 0.1 ms late, so they suit clocks up to a few kHz.

Run from the repository root: python tools/generate_logic_chip_parts.py
"""
import json
import os

from spice_levels import share_levels

FOLDER = "resources/library/logic_chips"
CATEGORY = "Logic chips (DIP)"
RON = 50  # ohm, each output switch


def vs():
    return "V({node:VCC},{node:GND})"


def level(pin):
    """0..1: the pin's voltage against VCC/2."""
    return f"0.5*(1+tanh(8*(V({{node:{pin}}},{{node:GND}})-0.5*{vs()})))"


def low(pin):
    return f"(1-{level(pin)})"


def dip_symbol(title, pins, bars=(), optional=None):
    """Pins in package order (pin 1 first); `bars`: active-low names drawn with a bar."""
    n = len(pins)
    half = n // 2
    height = 2 * half
    top = -half
    graphics = [
        {"rect": [-4, top, 8, height]},
        {"arc": [0, top, 0.7, 180, 180]},
        {"text": title, "at": [0, top + height / 2], "size": 0.8},
    ]
    result = []
    for i, name in enumerate(pins):
        number = i + 1
        if i < half:
            x_tip, x_edge, y, direction = -6, -4, top + 1 + 2 * i, "left"
        else:
            x_tip, x_edge, y, direction = 6, 4, top + 1 + 2 * (n - 1 - i), "right"
        graphics.append({"line": [x_tip, y, x_edge, y]})
        label_x = x_edge + (1.2 if x_edge < 0 else -1.2)
        shown = name[:-1] if name.endswith("N") and name in bars else name
        graphics.append({"text": shown, "at": [label_x, y], "size": 0.55})
        if name in bars:
            width = 0.17 * len(shown)
            graphics.append({"line": [label_x - width, y - 0.5, label_x + width, y - 0.5]})
        graphics.append({"text": str(number), "at": [(x_tip + x_edge) / 2, y - 0.5], "size": 0.45})
        pin = {"name": name, "at": [x_tip, y], "dir": direction}
        if optional is None or name in optional:
            if name not in ("VCC", "GND", "VDD", "VSS"):
                pin["optional"] = True
        result.append(pin)
    return {"graphics": graphics, "pins": result}, top


def driver(tag, value, pin, enable=None):
    """`pin` driven to VCC when `value` (0..1) is high, to GND when low; high impedance while `enable` is 0."""
    lines = [
        f"B_{{ref}}_x{tag} x{tag}_{{ref}} 0 V={value}",
        f"R_{{ref}}_d{tag} x{tag}_{{ref}} y{tag}_{{ref}} 1k",
        f"C_{{ref}}_d{tag} y{tag}_{{ref}} 0 10p",
    ]
    # Clamped: trapezoidal integration (transient mode) may ring on the 10 ns delay at microsecond steps.
    on = f"min(max(V(y{tag}_{{ref}}),0),1)"
    gate = f"{enable}*" if enable else ""
    lines += [
        f"B_{{ref}}_u{tag} {{node:VCC}} {{node:{pin}}} I={gate}{on}*V({{node:VCC}},{{node:{pin}}})/{RON}",
        f"B_{{ref}}_l{tag} {{node:{pin}}} {{node:GND}} I={gate}(1-{on})*V({{node:{pin}}},{{node:GND}})/{RON}",
    ]
    return lines


def inputs(pins):
    return [f"R_{{ref}}_i{p.lower()} {{node:{p}}} {{node:GND}} 1G" for p in pins]


def supply_current():
    return ["R_{ref}_q {node:VCC} {node:GND} 10Meg"]


def part(type_, name, description, symbol, top, spice, device=None):
    result = {
        "type": type_,
        "name": name,
        "category": CATEGORY,
        "prefix": "U",
        "description": description,
        "symbol": symbol,
        "designator_offset": [-1, top - 2],
        "value_offset": [-1, -top + 2],
    }
    if device:
        result["device"] = device
    result["spice"] = spice
    return result


# --- gates -------------------------------------------------------------------

QUAD_2_INPUT = ["1A", "1B", "1Y", "2A", "2B", "2Y", "GND", "3Y", "3A", "3B", "4Y", "4A", "4B", "VCC"]
QUAD_NOR = ["1Y", "1A", "1B", "2Y", "2A", "2B", "GND", "3A", "3B", "3Y", "4A", "4B", "4Y", "VCC"]
HEX_INVERTER = ["1A", "1Y", "2A", "2Y", "3A", "3Y", "GND", "4Y", "4A", "5Y", "5A", "6Y", "6A", "VCC"]

FUNCTIONS = {
    "nand": lambda a, b: f"(1-{a}*{b})",
    "and": lambda a, b: f"{a}*{b}",
    "nor": lambda a, b: f"(1-{a})*(1-{b})",
    "or": lambda a, b: f"(1-(1-{a})*(1-{b}))",
    "xor": lambda a, b: f"({a}+{b}-2*{a}*{b})",
}


def quad_gate(number, function, title, pins):
    spice = []
    for g in range(1, 5):
        a, b = level(f"{g}A"), level(f"{g}B")
        spice += driver(str(g), FUNCTIONS[function](a, b), f"{g}Y")
    spice += inputs([p for p in pins if p.endswith(("A", "B"))]) + supply_current()
    symbol, top = dip_symbol(f"74HC{number}", pins)
    return part(f"74hc{number}", f"74HC{number} {title}", f"Four 2-input {function.upper()} gates (DIP-14); wire VCC "
                f"(2 to 6 V) and GND", symbol, top, share_levels(spice))


def hex_inverter():
    spice = []
    for g in range(1, 7):
        spice += driver(str(g), low(f"{g}A"), f"{g}Y")
    spice += inputs([f"{g}A" for g in range(1, 7)]) + supply_current()
    symbol, top = dip_symbol("74HC04", HEX_INVERTER)
    return part("74hc04", "74HC04 hex inverter", "Six inverters (DIP-14); wire VCC (2 to 6 V) and GND", symbol, top,
                share_levels(spice))


def hex_schmitt():
    spice = []
    for g in range(1, 7):
        state = f"st{g}_{{ref}}"
        # A Schmitt trigger: the threshold is 0.53 VCC while the input is taken as low, 0.31 VCC while high
        # (NXP typical values at 4.5 V and 6 V).
        threshold = f"(0.31+0.22*(1-V({state})))*{vs()}"
        spice += [
            f"B_{{ref}}_st{g} 0 {state} I=1m*(0.5*(1+tanh(100*(V({{node:{g}A}},{{node:GND}})-{threshold})))-V({state}))",
            f"C_{{ref}}_st{g} {state} 0 1p",
        ]
        spice += driver(str(g), f"(1-V({state}))", f"{g}Y")
    spice += inputs([f"{g}A" for g in range(1, 7)]) + supply_current()
    symbol, top = dip_symbol("74HC14", HEX_INVERTER)
    return part("74hc14", "74HC14 hex Schmitt inverter",
                "Six inverters with Schmitt-trigger inputs (DIP-14): about 0.53 VCC to switch on rising, 0.31 VCC "
                "on falling; one with a resistor and a capacitor makes an RC oscillator", symbol, top, spice)


# --- decoder, multiplexer, transceiver ---------------------------------------------

def decoder_138():
    pins = ["A0", "A1", "A2", "E1N", "E2N", "E3", "Y7N", "GND", "Y6N", "Y5N", "Y4N", "Y3N", "Y2N", "Y1N", "Y0N",
            "VCC"]
    enable = f"{low('E1N')}*{low('E2N')}*{level('E3')}"
    spice = []
    for n in range(8):
        select = "*".join(level(f"A{b}") if n >> b & 1 else low(f"A{b}") for b in range(3))
        spice += driver(f"y{n}", f"(1-{enable}*{select})", f"Y{n}N")
    spice += inputs(["A0", "A1", "A2", "E1N", "E2N", "E3"]) + supply_current()
    bars = {"E1N", "E2N"} | {f"Y{n}N" for n in range(8)}
    symbol, top = dip_symbol("74HC138", pins, bars)
    return part("74hc138", "74HC138 3-to-8 decoder",
                "3-to-8 line decoder (DIP-16): with E1 and E2 low and E3 high, the output numbered by A2 A1 A0 goes "
                "low; the others stay high", symbol, top, share_levels(spice))


def mux_157():
    pins = ["S", "1I0", "1I1", "1Y", "2I0", "2I1", "2Y", "GND", "3Y", "3I1", "3I0", "4Y", "4I1", "4I0", "EN", "VCC"]
    spice = []
    for g in range(1, 5):
        value = f"{low('EN')}*({low('S')}*{level(f'{g}I0')}+{level('S')}*{level(f'{g}I1')})"
        spice += driver(str(g), value, f"{g}Y")
    spice += inputs([p for p in pins if "I" in p or p in ("S", "EN")]) + supply_current()
    symbol, top = dip_symbol("74HC157", pins, {"EN"})
    return part("74hc157", "74HC157 quad 2-to-1 multiplexer",
                "Four 2-input multiplexers (DIP-16): with E low each Y follows I0 while S is low and I1 while S is "
                "high; E high makes every Y low", symbol, top, share_levels(spice))


def transceiver_245():
    pins = ["DIR"] + [f"A{i}" for i in range(1, 9)] + ["GND"] + [f"B{i}" for i in range(8, 0, -1)] + ["OEN", "VCC"]
    a_to_b = f"{low('OEN')}*{level('DIR')}"
    b_to_a = f"{low('OEN')}*{low('DIR')}"
    spice = []
    for i in range(1, 9):
        spice += driver(f"b{i}", level(f"A{i}"), f"B{i}", a_to_b)
        spice += driver(f"a{i}", level(f"B{i}"), f"A{i}", b_to_a)
    spice += inputs([p for p in pins if p not in ("VCC", "GND")]) + supply_current()
    symbol, top = dip_symbol("74HC245", pins, {"OEN"})
    return part("74hc245", "74HC245 octal bus transceiver",
                "Eight bidirectional buffers (DIP-20): with OE low, DIR high drives B from A and DIR low drives A from "
                "B; OE high leaves both sides in high impedance", symbol, top, share_levels(spice))


# --- dual D flip-flop ------------------------------------------------------------------

def flipflop_74():
    pins = ["1CLRN", "1D", "1CLK", "1PREN", "1Q", "1QN", "GND", "2QN", "2Q", "2PREN", "2CLK", "2D", "2CLRN", "VCC"]
    spice = []
    for f in (1, 2):
        clock_low = f"(1-0.5*(1+tanh(8*(V({{node:{f}CLK}},{{node:GND}})-0.4*{vs()}))))"
        clock_high = f"0.5*(1+tanh(8*(V({{node:{f}CLK}},{{node:GND}})-0.6*{vs()})))"
        preset, clear = low(f"{f}PREN"), low(f"{f}CLRN")
        hold = f"(1-{preset})*(1-{clear})"
        master, slave = f"m{f}_{{ref}}", f"s{f}_{{ref}}"
        # Master follows D while the clock is low, the slave copies it while high; PRE and CLR act at once.
        spice += [
            f"B_{{ref}}_m{f} 0 {master} I=1m*({hold}*{clock_low}*({level(f'{f}D')}-V({master}))"
            f"+{preset}*(1-V({master}))-{clear}*V({master}))",
            f"C_{{ref}}_m{f} {master} 0 1p",
            f"B_{{ref}}_s{f} 0 {slave} I=1m*({hold}*{clock_high}*(V({master})-V({slave}))"
            f"+{preset}*(1-V({slave}))-{clear}*V({slave}))",
            f"C_{{ref}}_s{f} {slave} 0 1p",
        ]
        q = f"0.5*(1+tanh(20*(V({slave})-0.5)))"
        spice += driver(f"q{f}", q, f"{f}Q") + driver(f"qn{f}", f"(1-{q})", f"{f}QN")
    spice += inputs(["1CLRN", "1D", "1CLK", "1PREN", "2PREN", "2CLK", "2D", "2CLRN"]) + supply_current()
    symbol, top = dip_symbol("74HC74", pins, {"1CLRN", "1PREN", "1QN", "2QN", "2PREN", "2CLRN"})
    return part("74hc74", "74HC74 dual D flip-flop",
                "Two D flip-flops (DIP-14): Q takes D at the rising edge of CLK; PRE low sets Q, CLR low clears it, "
                "at once; tie unused PRE and CLR to VCC; both start with Q low", symbol, top, share_levels(spice))


# --- counters and shift registers (devices) -----------------------------------------------

def device_spice(pins, outputs, supply="VCC", ground="GND"):
    signals = [p for p in pins if p not in (supply, ground)]
    spice = [f"R_{{ref}}_h{p.lower()} {{node:{p}}} {{node:{ground}}} 100Meg" for p in signals]
    for p in outputs:
        n = p.lower()
        spice.append(f"B_{{ref}}_{n} {{node:{ground}}} {{node:{p}}} "
                     f"I=V({{ext:{n}_i}})-V({{ext:{n}_g}})*(V({{node:{p}}})-V({{node:{ground}}}))")
    spice.append(f"R_{{ref}}_q {{node:{supply}}} {{node:{ground}}} 1Meg")
    return spice


DEVICE_NOTE = ("; runs as a digital chip: with a board or on its own, inputs from the rest of the circuit arrive up "
               "to 0.1 ms late (clocks up to a few kHz)")


def counter_161():
    pins = ["MRN", "CP", "D0", "D1", "D2", "D3", "CEP", "GND", "PEN", "CET", "Q3", "Q2", "Q1", "Q0", "TC", "VCC"]
    outputs = ["Q0", "Q1", "Q2", "Q3", "TC"]
    symbol, top = dip_symbol("74HC161", pins, {"MRN", "PEN"})
    return part("74hc161", "74HC161 4-bit binary counter",
                "Synchronous 4-bit counter (DIP-16): counts at the rising edge of CP while CEP and CET are high, "
                "loads D0-D3 at the edge while PE is low, MR low clears it at once; TC is high at 15 while CET is "
                "high" + DEVICE_NOTE,
                symbol, top, device_spice(pins, outputs),
                {"kind": "74hc161", "supply_pin": "VCC", "ground_pin": "GND", "output_resistance": RON,
                 "driven_pins": outputs})


def counter_193():
    pins = ["D1", "Q1", "Q0", "CPD", "CPU", "Q2", "Q3", "GND", "D3", "D2", "PLN", "TCUN", "TCDN", "MR", "D0", "VCC"]
    outputs = ["Q0", "Q1", "Q2", "Q3", "TCUN", "TCDN"]
    symbol, top = dip_symbol("74HC193", pins, {"PLN", "TCUN", "TCDN"})
    return part("74hc193", "74HC193 4-bit up/down counter",
                "Presettable up/down counter (DIP-16): counts up at the rising edge of CPU while CPD is high and down "
                "at the rising edge of CPD while CPU is high; PL low loads D0-D3 and MR high clears it, at once; TCU "
                "and TCD go low at 15 (CPU low) and at 0 (CPD low) for cascading" + DEVICE_NOTE,
                symbol, top, device_spice(pins, outputs),
                {"kind": "74hc193", "supply_pin": "VCC", "ground_pin": "GND", "output_resistance": RON,
                 "driven_pins": outputs})


def shift_165():
    pins = ["PLN", "CP", "D4", "D5", "D6", "D7", "Q7N", "GND", "Q7", "DS", "D0", "D1", "D2", "D3", "CEN", "VCC"]
    outputs = ["Q7", "Q7N"]
    symbol, top = dip_symbol("74HC165", pins, {"PLN", "Q7N", "CEN"})
    return part("74hc165", "74HC165 8-bit parallel-in shift register",
                "Parallel-in, serial-out shift register (DIP-16): PL low loads D0-D7; with PL high each rising edge "
                "of CP while CE is low shifts toward Q7 and takes DS into the first stage; reads switches into a "
                "microcontroller's SPI" + DEVICE_NOTE,
                symbol, top, device_spice(pins, outputs),
                {"kind": "74hc165", "supply_pin": "VCC", "ground_pin": "GND", "output_resistance": RON,
                 "driven_pins": outputs})


def counter_4017():
    pins = ["Q5", "Q1", "Q0", "Q2", "Q6", "Q7", "Q3", "VSS", "Q8", "Q4", "Q9", "CO", "CP1", "CP0", "MR", "VDD"]
    outputs = [f"Q{i}" for i in range(10)] + ["CO"]
    symbol, top = dip_symbol("CD4017", pins)
    return part("cd4017", "CD4017 decade counter with 10 outputs",
                "Johnson decade counter (DIP-16): one of Q0-Q9 is high, the next at each rising edge of CP0 while CP1 "
                "is low (or falling CP1 while CP0 is high); MR high returns to Q0; CO is high for counts 0-4; VDD 3 "
                "to 15 V" + DEVICE_NOTE,
                symbol, top, device_spice(pins, outputs, "VDD", "VSS"),
                {"kind": "cd4017", "supply_pin": "VDD", "ground_pin": "VSS", "output_resistance": 200,
                 "driven_pins": outputs})


PARTS = [
    lambda: quad_gate("00", "nand", "quad NAND gate", QUAD_2_INPUT),
    lambda: quad_gate("02", "nor", "quad NOR gate", QUAD_NOR),
    hex_inverter,
    lambda: quad_gate("08", "and", "quad AND gate", QUAD_2_INPUT),
    hex_schmitt,
    lambda: quad_gate("32", "or", "quad OR gate", QUAD_2_INPUT),
    lambda: quad_gate("86", "xor", "quad XOR gate", QUAD_2_INPUT),
    flipflop_74,
    decoder_138,
    mux_157,
    transceiver_245,
    counter_161,
    counter_193,
    shift_165,
    counter_4017,
]


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
