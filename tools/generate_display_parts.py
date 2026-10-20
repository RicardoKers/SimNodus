"""Writes the 7-segment displays and decoders of resources/library/display
(SN-202, revised after the owner's review of 2026-10-19).

Displays: segments a-g and the decimal point are LEDs with a common cathode
or a common anode; each segment has a 0 V measuring source, so the editor
lights each drawn segment with its own current. The pins are on the top and
the bottom edge as on a real 10-pin display (g f COM a b above, e d COM c dp
below), so displays can stand side by side for several digits.

Decoders have every pin of the real part, power included, because students
wire real circuits: the outputs take their current from the supply pins and
nothing works without them.
- 4511 (CMOS, for a common cathode): A-D, LT (lamp test, low = all segments
  on), BI (blanking, low = all off), LE (latch enable, high = hold the code),
  a-g sourcing current from VDD, VDD and VSS; codes 10-15 blank; 6 and 9
  without tails, as the chip draws them.
- 7447 (TTL, for a common anode): A-D, LT, RBI (ripple blanking in: low blanks
  a zero), BI/RBO (low blanks everything; driven low when a zero is blanked,
  for leading-zero blanking of several digits), open-collector outputs a-g that
  sink the segment current, VCC and GND; inputs float high like TTL; codes
  10-15 give the chip's own symbols; about 40 mA supply current.
- Two generic hex decoders (0-9, A, b, C, d, E, F) with push-pull outputs, for
  a common cathode (high = on) and a common anode (low = on).

Logic levels follow the supply pins: CMOS inputs switch at half the supply,
TTL inputs at 1.4 V. Outputs change with about 10 ns of delay.

Multiplexed displays (SN-204), for the MAX7219 or a microcontroller scanning
them: a 4-digit 7-segment display with a common cathode per digit (pins as on a
3641AS: D1 a f D2 D3 b above, e d dp c g D4 below) and an 8x8 LED matrix with
the cathodes on the rows R1-R8 and the anodes on the columns C1-C8 (a MAX7219
drives the rows from DIG0-DIG7 and the columns from DP, A-G, so a byte written
for a row shows with its most significant bit on the left). Each LED
lights with its own current; the editor averages it over a few milliseconds,
as the eye does.

Run from the repository root: python tools/generate_display_parts.py
"""
import json
import os

FOLDER = "resources/library/display"
SEGMENTS = ["a", "b", "c", "d", "e", "f", "g"]
# Segments of 0-9 as the 4511 and the 7447 draw them (6 and 9 without tails).
CHIP_DIGITS = {
    0: "abcdef", 1: "bc", 2: "abdeg", 3: "abcdg", 4: "bcfg",
    5: "acdfg", 6: "cdefg", 7: "abc", 8: "abcdefg", 9: "abcfg",
}
# The 7447's symbols for 10-15 (the last one is blank).
TTL_EXTRA = {10: "deg", 11: "cdg", 12: "bfg", 13: "adfg", 14: "defg", 15: ""}
# Hexadecimal digits of the generic decoders (6 and 9 with tails).
HEX_DIGITS = {
    0: "abcdef", 1: "bc", 2: "abdeg", 3: "abcdg", 4: "bcfg", 5: "acdfg", 6: "acdefg", 7: "abc",
    8: "abcdefg", 9: "abcdfg", 10: "abcefg", 11: "cdefg", 12: "adef", 13: "bcdeg", 14: "adefg", 15: "aefg",
}
LED_MODELS = [
    ".model LED_red D(IS=1.2e-18 N=2 RS=5 BV=5 IBV=10u)",
    ".model LED_yellow D(IS=1.7e-19 N=2 RS=5 BV=5 IBV=10u)",
    ".model LED_green D(IS=2.5e-20 N=2 RS=5 BV=5 IBV=10u)",
    ".model LED_blue D(IS=6.7e-28 N=2 RS=8 BV=5 IBV=10u)",
    ".model LED_white D(IS=6.7e-28 N=2 RS=8 BV=5 IBV=10u)",
]


# ---- Displays -------------------------------------------------------------

def horizontal(y):
    return {"polygon": [-1.6, y, -1.3, y - 0.3, 1.3, y - 0.3, 1.6, y, 1.3, y + 0.3, -1.3, y + 0.3]}


def vertical(x, top, bottom):
    return {"polygon": [x, top, x + 0.3, top + 0.3, x + 0.3, bottom - 0.3, x, bottom, x - 0.3, bottom - 0.3,
                        x - 0.3, top + 0.3]}


def display(common_anode):
    kind = "common anode" if common_anode else "common cathode"
    graphics = [{"rect": [-5, -6, 10, 12]}]
    shapes = {
        "a": horizontal(-4.2), "g": horizontal(0), "d": horizontal(4.2),
        "f": vertical(-1.9, -3.9, -0.3), "b": vertical(1.9, -3.9, -0.3),
        "e": vertical(-1.9, 0.3, 3.9), "c": vertical(1.9, 0.3, 3.9),
    }
    index = {}
    for s in SEGMENTS:
        index[s] = len(graphics)
        graphics.append(shapes[s])
    index["dp"] = len(graphics)
    graphics.append({"circle": [2.9, 4.2, 0.35]})
    # Pins as on a real display: g f COM a b on top, e d COM c dp below.
    top = ["g", "f", "COM", "a", "b"]
    bottom = ["e", "d", "COM2", "c", "dp"]
    pins = []
    for row, names, edge, tip, direction in ((top, top, -6, -8, "up"), (bottom, bottom, 6, 8, "down")):
        for i, name in enumerate(names):
            x = -4 + 2 * i
            pin = {"name": name, "at": [x, tip], "dir": direction}
            if name in ("COM", "COM2", "dp"):
                pin["optional"] = True  # either common pin may be used; the decimal point may stay unused
            pins.append(pin)
            graphics.append({"line": [x, tip, x, edge]})
            label = "COM" if name == "COM2" else name
            graphics.append({"text": label if len(label) < 3 else "C", "at": [x, edge + (0.7 if edge < 0 else -0.7)],
                             "size": 0.55})
    graphics.append({"text": "CA" if common_anode else "CC", "at": [-3.5, 4.6], "size": 0.55})
    spice = ["R_{ref}_com {node:COM} {node:COM2} 1m"]  # the two common pins are joined inside
    lights = []
    for s in SEGMENTS + ["dp"]:
        source = f"V_{{ref}}_s{s}"
        node = f"s{s}_{{ref}}"
        if common_anode:  # COM -> measuring source -> LED -> segment pin
            spice += [f"{source} {{node:COM}} {node} DC 0", f"D_{{ref}}_{s} {node} {{node:{s}}} LED_{{color}}"]
        else:  # segment pin -> measuring source -> LED -> COM
            spice += [f"{source} {{node:{s}}} {node} DC 0", f"D_{{ref}}_{s} {node} {{node:COM}} LED_{{color}}"]
        lights.append({"element": source, "graphic": index[s]})
    return {
        "type": "seven_segment_ca" if common_anode else "seven_segment_cc",
        "name": f"7-segment display ({kind})",
        "category": "Displays",
        "prefix": "DS",
        "description": f"7-segment LED display, {kind}, pins on top (g f COM a b) and below (e d COM c dp) as on a "
                       "real display, so several stand side by side; "
                       + ("COM goes to the supply and a low segment pin lights the segment (a 7447 drives it)"
                          if common_anode else "COM goes to ground and a high segment pin lights the segment (a 4511 "
                                               "drives it)")
                       + "; each segment lights with its own current, fully at 10 mA; use a resistor per segment",
        "symbol": {"graphics": graphics, "pins": pins},
        # Above and below the middle pins: displays stand side by side.
        "designator_offset": [1, -10],
        "value_offset": [1, 10],
        "value_parameter": "color",
        "parameters": [{"name": "color", "label": "Color", "unit": "", "default": "red",
                        "description": "Segment color; sets the forward voltage (red about 1.9 V at 10 mA)",
                        "choices": ["red", "yellow", "green", "blue", "white"]}],
        "lights": {"full": "10m", "color_parameter": "color", "segments": lights},
        "spice": spice,
        "models": LED_MODELS,
    }


# ---- Decoders ---------------------------------------------------------------

def cmos(pin, vdd, vss):
    """0..1: the pin against half of the supply."""
    return (f"0.5*(1+tanh(20*(V({{node:{pin}}},{{node:{vss}}})/max(V({{node:{vdd}}},{{node:{vss}}}),0.5)-0.5)))")


def ttl(pin, gnd):
    """0..1: the pin against the TTL threshold of 1.4 V."""
    return f"0.5*(1+tanh(10*(V({{node:{pin}}},{{node:{gnd}}})-1.4)))"


def minterms(levels, digits):
    """A node m<d> (0..1) for each code; `levels` are the four bit nodes."""
    lines = []
    for code in sorted(digits):
        factors = [f"V({levels[i]})" if (code >> i) & 1 else f"(1-V({levels[i]}))" for i in range(4)]
        lines.append(f"B_{{ref}}_m{code} m{code}_{{ref}} 0 V={'*'.join(factors)}")
    return lines


def segment_sum(segment, digits):
    terms = [f"V(m{code}_{{ref}})" for code, lit in digits.items() if segment in lit]
    return f"min({'+'.join(terms)},1)" if terms else "0"


def delayed(segment, value):
    """The segment's on state (0..1) after about 10 ns: node y<s>."""
    return [
        f"B_{{ref}}_x{segment} x{segment}_{{ref}} 0 V={value}",
        f"R_{{ref}}_d{segment} x{segment}_{{ref}} y{segment}_{{ref}} 1k",
        f"C_{{ref}}_d{segment} y{segment}_{{ref}} 0 10p",
    ]


def push_pull(segment, high_on, vdd, vss):
    """An output that connects to VDD or VSS through about 50 ohm."""
    on = f"V(y{segment}_{{ref}})"
    up, down = (on, f"(1-{on})") if high_on else (f"(1-{on})", on)
    return [
        f"B_{{ref}}_u{segment} {{node:{vdd}}} {{node:{segment}}} I={up}*V({{node:{vdd}}},{{node:{segment}}})/50",
        f"B_{{ref}}_w{segment} {{node:{segment}}} {{node:{vss}}} I={down}*V({{node:{segment}}},{{node:{vss}}})/50",
    ]


def open_collector(segment, gnd):
    """Sinks current to GND through about 20 ohm while the segment is on; open otherwise."""
    return [f"B_{{ref}}_w{segment} {{node:{segment}}} {{node:{gnd}}} "
            f"I=V(y{segment}_{{ref}})*max(V({{node:{segment}}},{{node:{gnd}}}),0)/20+V({{node:{segment}}},{{node:{gnd}}})*1e-9"]


def decoder_symbol(title, left, right, top, bottom, inverted=(), optional=()):
    rows = max(len(left), len(right))
    height = 2 * rows + 2
    upper = -(height // 2)
    graphics = [{"rect": [-4, upper, 8, height]}, {"text": title, "at": [0, upper + 1.2], "size": 0.8}]
    pins = []
    for names, x_tip, x_edge, direction in ((left, -6, -4, "left"), (right, 6, 4, "right")):
        first = -(len(names) - 1)
        for i, name in enumerate(names):
            y = first + 2 * i
            graphics.append({"line": [x_tip, y, x_edge, y]})
            shown = name.replace("/", "/")
            graphics.append({"text": shown, "at": [x_edge + (1.2 if x_edge < 0 else -1.0), y], "size": 0.6})
            if name in inverted:  # active low: a bubble at the body
                cx = x_edge - 0.3 if x_edge < 0 else x_edge + 0.3
                graphics.append({"circle": [cx, y, 0.3]})
            pin = {"name": name, "at": [x_tip, y], "dir": direction}
            if name in optional:
                pin["optional"] = True
            pins.append(pin)
    graphics += [{"line": [0, upper - 2, 0, upper]}, {"text": top, "at": [0, upper + 0.5], "size": 0.55},
                 {"line": [0, -upper, 0, -upper + 2]}, {"text": bottom, "at": [0, -upper - 0.5], "size": 0.55}]
    pins += [{"name": top, "at": [0, upper - 2], "dir": "up"}, {"name": bottom, "at": [0, -upper + 2], "dir": "down"}]
    return {"graphics": graphics, "pins": pins}, upper


def decoder_part(type_, name, description, symbol, upper, spice):
    return {
        "type": type_,
        "name": name,
        "category": "Displays",
        "prefix": "U",
        "description": description,
        "symbol": symbol,
        "designator_offset": [5, upper - 1],
        "value_offset": [5, -upper + 1],
        "spice": spice,
    }


def decoder_4511():
    vdd, vss = "VDD", "VSS"
    spice = []
    # Input latches: while LE is low each bit follows its input; while LE is high it holds.
    hold = cmos("LE", vdd, vss)
    for i, pin in enumerate("ABCD"):
        spice += [f"B_{{ref}}_l{i} 0 l{i}_{{ref}} I=1m*(1-{hold})*({cmos(pin, vdd, vss)}-V(l{i}_{{ref}}))",
                  f"C_{{ref}}_l{i} l{i}_{{ref}} 0 1p"]
    spice += minterms([f"l{i}_{{ref}}" for i in range(4)], CHIP_DIGITS)
    test = f"(1-{cmos('LT', vdd, vss)})"
    blank = f"(1-{cmos('BI', vdd, vss)})"
    for s in SEGMENTS:
        spice += delayed(s, f"{test}+(1-{test})*(1-{blank})*{segment_sum(s, CHIP_DIGITS)}")
        spice += push_pull(s, True, vdd, vss)
    spice += [f"R_{{ref}}_i{p} {{node:{p}}} {{node:VSS}} 1G" for p in ["A", "B", "C", "D", "LT", "BI", "LE"]]
    spice.append("R_{ref}_q {node:VDD} {node:VSS} 100k")
    symbol, upper = decoder_symbol("4511", ["A", "B", "C", "D", "LT", "BI", "LE"], SEGMENTS, "VDD", "VSS",
                                   inverted=("LT", "BI"))
    return decoder_part("decoder_4511", "BCD to 7-segment decoder (4511)",
                        "CMOS BCD to 7-segment latch/decoder/driver like the CD4511 for a common-cathode display: "
                        "outputs a-g source current from VDD (high = segment on, one resistor per segment); LT low "
                        "lights every segment, BI low blanks them, LE high holds the last code; codes 10-15 are "
                        "blank; tie LT and BI high and LE low for normal use; supply 3-15 V on VDD and VSS",
                        symbol, upper, spice)


def decoder_7447():
    vcc, gnd = "VCC", "GND"
    spice = []
    for i, pin in enumerate("ABCD"):
        spice.append(f"B_{{ref}}_l{i} l{i}_{{ref}} 0 V={ttl(pin, gnd)}")
    digits = dict(CHIP_DIGITS)
    digits.update(TTL_EXTRA)
    spice += minterms([f"l{i}_{{ref}}" for i in range(4)], digits)
    test = f"(1-{ttl('LT', gnd)})"
    blank = f"(1-{ttl('BI/RBO', gnd)})"
    for s in SEGMENTS:
        spice += delayed(s, f"(1-{blank})*({test}+(1-{test})*{segment_sum(s, digits)})")
        spice += open_collector(s, gnd)
    # RBO: a zero is blanked (and BI/RBO pulled low) while RBI is low and LT is high.
    zero = f"(1-{ttl('RBI', gnd)})*{ttl('LT', gnd)}*V(m0_{{ref}})"
    spice.append(f"B_{{ref}}_rbo {{node:BI/RBO}} {{node:GND}} I={zero}*max(V({{node:BI/RBO}},{{node:GND}}),0)/100")
    # TTL inputs float high: pull-ups to VCC; the supply draws about 40 mA.
    spice += [f"R_{{ref}}_p{i} {{node:{p}}} {{node:VCC}} 4k" for i, p in enumerate(["A", "B", "C", "D", "LT", "RBI", "BI/RBO"])]
    spice.append("R_{ref}_q {node:VCC} {node:GND} 125")
    symbol, upper = decoder_symbol("7447", ["A", "B", "C", "D", "LT", "RBI", "BI/RBO"], SEGMENTS, "VCC", "GND",
                                   inverted=("LT", "RBI", "BI/RBO") + tuple(SEGMENTS),
                                   optional=("LT", "RBI", "BI/RBO"))
    return decoder_part("decoder_7447", "BCD to 7-segment decoder (7447)",
                        "TTL BCD to 7-segment decoder/driver like the 7447 for a common-anode display: open-collector "
                        "outputs a-g sink the segment current (low = on, one resistor per segment to the display); LT "
                        "low lights every segment, BI/RBO low blanks them, RBI low blanks a zero and pulls RBO low "
                        "(leading-zero blanking over several digits); codes 10-15 give the chip's symbols, 15 blank; "
                        "unconnected inputs float high as in TTL; supply 5 V on VCC and GND, about 40 mA",
                        symbol, upper, spice)


def decoder_hex(common_anode):
    vcc, gnd = "VCC", "GND"
    spice = []
    for i, pin in enumerate("ABCD"):
        spice.append(f"B_{{ref}}_l{i} l{i}_{{ref}} 0 V={cmos(pin, vcc, gnd)}")
    spice += minterms([f"l{i}_{{ref}}" for i in range(4)], HEX_DIGITS)
    for s in SEGMENTS:
        spice += delayed(s, segment_sum(s, HEX_DIGITS))
        spice += push_pull(s, not common_anode, vcc, gnd)
    spice += [f"R_{{ref}}_i{p} {{node:{p}}} {{node:GND}} 1G" for p in "ABCD"]
    spice.append("R_{ref}_q {node:VCC} {node:GND} 100k")
    kind = "common anode" if common_anode else "common cathode"
    symbol, upper = decoder_symbol("HEX", ["A", "B", "C", "D"], SEGMENTS, "VCC", "GND",
                                   inverted=tuple(SEGMENTS) if common_anode else ())
    return decoder_part("decoder_hex_ca" if common_anode else "decoder_hex_cc",
                        f"Hex to 7-segment decoder ({kind})",
                        f"Generic 4-bit decoder for a {kind} display: shows 0-9 and A, b, C, d, E, F; push-pull outputs "
                        + ("low = segment on" if common_anode else "high = segment on")
                        + " through about 50 ohm from the supply (one resistor per segment); inputs switch at half "
                          "the supply on VCC and GND",
                        symbol, upper, spice)


# ---- Multiplexed displays (SN-204) ------------------------------------------

# A multiplexed LED is reverse biased by up to the full supply while its row is
# off (a MAX7219 pulls unused digits to V+ and unlit segments to ground): these
# models break down at 10 V instead of 5 V, as display LEDs are rated. They have
# no series resistance (a few ohms beside the drivers' resistance), which saves
# the engine an internal node per LED.
MUX_LED_MODELS = [m.replace(".model LED_", ".model MLED_").replace("BV=5", "BV=10").replace(" RS=5", "").replace(" RS=8", "")
                  for m in LED_MODELS]


def shifted(shape, dx):
    key, values = next(iter(shape.items()))
    return {key: [v + dx if i % 2 == 0 else v for i, v in enumerate(values)]}


def four_digit():
    graphics = [{"rect": [-12, -6, 24, 12]}]
    shapes = {
        "a": horizontal(-4.2), "g": horizontal(0), "d": horizontal(4.2),
        "f": vertical(-1.9, -3.9, -0.3), "b": vertical(1.9, -3.9, -0.3),
        "e": vertical(-1.9, 0.3, 3.9), "c": vertical(1.9, 0.3, 3.9),
    }
    lights = []
    spice = []
    for k in range(4):
        dx = -9 + 6 * k
        for s in SEGMENTS + ["dp"]:
            source = f"V_{{ref}}_{s}{k + 1}"
            node = f"{s}{k + 1}_{{ref}}"
            spice += [f"{source} {{node:{s}}} {node} DC 0", f"D_{{ref}}_{s}{k + 1} {node} {{node:D{k + 1}}} MLED_{{color}}"]
            lights.append({"element": source, "graphic": len(graphics)})
            graphics.append(shifted(shapes[s], dx) if s != "dp" else {"circle": [dx + 2.9, 4.2, 0.35]})
    top = ["D1", "a", "f", "D2", "D3", "b"]
    bottom = ["e", "d", "dp", "c", "g", "D4"]
    pins = []
    for names, edge, tip, direction in ((top, -6, -8, "up"), (bottom, 6, 8, "down")):
        for i, name in enumerate(names):
            x = -10 + 4 * i
            pin = {"name": name, "at": [x, tip], "dir": direction}
            if name == "dp":
                pin["optional"] = True
            pins.append(pin)
            graphics.append({"line": [x, tip, x, edge]})
            graphics.append({"text": name, "at": [x + 0.9, edge + (0.8 if edge < 0 else -0.8)], "size": 0.5})
    return {
        "type": "seven_segment_4digit_cc",
        "name": "4-digit 7-segment display (common cathode)",
        "category": "Displays",
        "prefix": "DS",
        "description": "Four 7-segment digits with shared segment pins a-g, dp and one common cathode per digit (D1-D4), "
                       "pins as on a 3641AS; light one digit at a time (multiplexing), from a MAX7219 or a "
                       "microcontroller scanning the digits; each segment lights with its own current, averaged as the "
                       "eye does",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-12, -10],
        "value_offset": [-12, 10],
        "value_parameter": "color",
        "parameters": [{"name": "color", "label": "Color", "unit": "", "default": "red",
                        "description": "Segment color; sets the forward voltage (red about 1.9 V at 10 mA)",
                        "choices": ["red", "yellow", "green", "blue", "white"]}],
        "lights": {"full": "10m", "color_parameter": "color", "segments": lights},
        "spice": spice,
        "models": MUX_LED_MODELS,
    }


def matrix_8x8():
    graphics = [{"rect": [-9, -9, 18, 18]}]
    lights = []
    spice = []
    for r in range(8):
        for c in range(8):
            source = f"V_{{ref}}_r{r + 1}c{c + 1}"
            node = f"r{r + 1}c{c + 1}_{{ref}}"
            spice += [f"{source} {{node:C{c + 1}}} {node} DC 0", f"D_{{ref}}_r{r + 1}c{c + 1} {node} {{node:R{r + 1}}} MLED_{{color}}"]
            lights.append({"element": source, "graphic": len(graphics)})
            graphics.append({"circle": [-7 + 2 * c, -7 + 2 * r, 0.75]})
    pins = []
    for r in range(8):
        y = -7 + 2 * r
        pins.append({"name": f"R{r + 1}", "at": [-11, y], "dir": "left"})
        graphics.append({"line": [-11, y, -9, y]})
    for c in range(8):
        x = -7 + 2 * c
        pins.append({"name": f"C{c + 1}", "at": [x, 11], "dir": "down"})
        graphics.append({"line": [x, 11, x, 9]})
    graphics.append({"text": "R", "at": [-10, -9.2], "size": 0.5})
    graphics.append({"text": "C", "at": [-9.2, 10], "size": 0.5})
    return {
        "type": "led_matrix_8x8",
        "name": "8x8 LED matrix",
        "category": "Displays",
        "prefix": "DS",
        "description": "64 LEDs in 8 rows and 8 columns: the cathodes of each row are joined (R1-R8), the anodes of each "
                       "column (C1-C8); light one row at a time (multiplexing), from a MAX7219 (DIG0-DIG7 to R1-R8, "
                       "DP, A-G to C1-C8) or a microcontroller; each LED lights with its own current, averaged as the "
                       "eye does",
        "symbol": {"graphics": graphics, "pins": pins},
        "designator_offset": [-9, -11],
        "value_offset": [3, -11],
        "value_parameter": "color",
        "parameters": [{"name": "color", "label": "Color", "unit": "", "default": "red",
                        "description": "LED color; sets the forward voltage (red about 1.9 V at 10 mA)",
                        "choices": ["red", "yellow", "green", "blue", "white"]}],
        "lights": {"full": "10m", "color_parameter": "color", "segments": lights},
        "spice": spice,
        "models": MUX_LED_MODELS,
    }


def main():
    os.makedirs(FOLDER, exist_ok=True)
    # Short descriptions for the parts tree (details belong in the user guide).
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "part_descriptions.json"), encoding="utf-8") as fh:
        short = json.load(fh)
    for p in [display(False), display(True), decoder_4511(), decoder_7447(), decoder_hex(False), decoder_hex(True),
              four_digit(), matrix_8x8()]:
        p["description"] = short.get(p["type"], p["description"])
        with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
