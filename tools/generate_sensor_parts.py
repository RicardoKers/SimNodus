"""Writes the sensors and transducers of resources/library/sensors (SN-221).

The quantity a sensor measures (temperature, light, magnetic field, load) is
turned while running like a potentiometer: drag the part or use the wheel. It
lives on an internal node q_<ref> whose voltage is the quantity in its unit,
shown on the symbol while running. Models follow the datasheets' typical
values; what they leave out is said in each description and in the user guide.

Run from the repository root: python tools/generate_sensor_parts.py
"""
import json
import math
import os

FOLDER = "resources/library/sensors"
CATEGORY = "Sensors"


def number(name, label, unit, default, description, minimum=None, maximum=None):
    p = {"name": name, "label": label, "unit": unit, "default": default, "description": description}
    if minimum is not None:
        p["min"] = minimum
    if maximum is not None:
        p["max"] = maximum
    return p


def quantity(name, label, unit, default, minimum, maximum, what):
    return number(name, label, unit, default,
                  f"{what}; drag the part or use the wheel while running ({minimum} to {maximum} {unit})",
                  minimum, maximum)


def linear(minimum, maximum):
    """The quantity node from the control (0..1)."""
    return f"B_{{ref}}_q q_{{ref}} 0 V={minimum}+({maximum}-({minimum}))*V({{control}})"


def logarithmic(minimum, maximum):
    return f"B_{{ref}}_q q_{{ref}} 0 V={minimum}*exp({math.log(maximum / minimum):.9f}*V({{control}}))"


def supply_ok(vcc, gnd, threshold):
    """About 1 with the supply above `threshold` volts, 0 below (smooth)."""
    return f"0.5*(1+tanh(5*(V({{node:{vcc}}},{{node:{gnd}}})-{threshold})))"


def readout(unit):
    return {"kind": "node", "element": "q_{ref}", "scale": 1, "unit": unit}


def pin(name, x, y, direction):
    return {"name": name, "at": [x, y], "dir": direction}


def chip_symbol(title, left=(), right=(), top=(), bottom=(), width=8, height=8):
    """A box with pins on its sides; each side lists (name, offset along it)."""
    w2, h2 = width / 2, height / 2
    graphics = [{"rect": [-w2, -h2, width, height]}, {"text": title, "at": [0, 0], "size": 0.8}]
    pins = []
    for name, y in left:
        graphics += [{"line": [-w2 - 2, y, -w2, y]}, {"text": name, "at": [-w2 + 1.1, y], "size": 0.55}]
        pins.append(pin(name, -w2 - 2, y, "left"))
    for name, y in right:
        graphics += [{"line": [w2 + 2, y, w2, y]}, {"text": name, "at": [w2 - 1.1, y], "size": 0.55}]
        pins.append(pin(name, w2 + 2, y, "right"))
    for name, x in top:
        graphics += [{"line": [x, -h2 - 2, x, -h2]}, {"text": name, "at": [x, -h2 + 0.6], "size": 0.5}]
        pins.append(pin(name, x, -h2 - 2, "up"))
    for name, x in bottom:
        graphics += [{"line": [x, h2 + 2, x, h2]}, {"text": name, "at": [x, h2 - 0.6], "size": 0.5}]
        pins.append(pin(name, x, h2 + 2, "down"))
    return {"graphics": graphics, "pins": pins}


# A two-pin resistor body (vertical, pins 1 at the top and 2 at the bottom)
# with a mark across it: the usual symbol of a resistance that follows a quantity.
def variable_resistor(mark):
    return {
        "graphics": [
            {"rect": [-0.6, -1.8, 1.2, 3.6]},
            {"line": [0, -3, 0, -1.8]},
            {"line": [0, 1.8, 0, 3]},
            {"line": [-1.4, 1.6, 1.0, -1.2]},
            {"line": [1.0, -1.2, 1.6, -1.2]},
        ] + mark,
        "pins": [pin("1", 0, -3, "up"), pin("2", 0, 3, "down")],
    }


LIGHT_ARROWS = [
    {"line": [-2.6, -1.8, -1.2, -0.8]},
    {"polygon": [-1.0, -0.6, -1.6, -0.8, -1.3, -1.2], "fill": True},
    {"line": [-2.6, -0.6, -1.2, 0.4]},
    {"polygon": [-1.0, 0.6, -1.6, 0.4, -1.3, 0.0], "fill": True},
]


def lm35():
    s = supply_ok("VS", "GND", 3.6)
    return {
        "type": "lm35",
        "name": "Temperature sensor LM35",
        "category": CATEGORY,
        "prefix": "U",
        "description": "Analog temperature sensor: 10 mV/°C at VOUT, supply 4 to 30 V; the basic circuit's output stays "
                       "at 0 V below 0 °C",
        "symbol": chip_symbol("LM35", right=[("VOUT", 0)], top=[("VS", 0)], bottom=[("GND", 0)]),
        "designator_offset": [5, -5],
        "value_offset": [5, 5],
        "value_parameter": "temperature",
        "parameters": [quantity("temperature", "Temperature", "°C", "25", "-55", "150", "Temperature of the sensor")],
        "interactive": {"kind": "potentiometer", "parameter": "temperature"},
        "readout": readout("°C"),
        "spice": [
            linear(-55, 150),
            f"B_{{ref}}_o o_{{ref}} {{node:GND}} V={s}*min(max(0.01*V(q_{{ref}}),0),"
            "max(V({node:VS},{node:GND})-1.5,0))",
            "R_{ref}_o o_{ref} {node:VOUT} 1",
            "R_{ref}_s {node:VS} {node:GND} 90k",
        ],
    }


def ntc():
    return {
        "type": "ntc",
        "name": "NTC thermistor",
        "category": CATEGORY,
        "prefix": "RT",
        "description": "Resistance that falls as the temperature rises (beta model); 10 kΩ at 25 °C by default; "
                       "no self-heating",
        "symbol": variable_resistor([{"text": "-t°", "at": [2.1, 1.2], "size": 0.6}]),
        "value_parameter": "temperature",
        "parameters": [
            number("R25", "Resistance at 25 °C", "Ohm", "10k", "Nominal resistance at 25 °C", "1", "10M"),
            number("B", "Beta", "K", "3950", "B constant of the datasheet (B25/50 or B25/85)", "1000", "10000"),
            quantity("temperature", "Temperature", "°C", "25", "-40", "125", "Temperature of the thermistor"),
        ],
        "interactive": {"kind": "potentiometer", "parameter": "temperature"},
        "readout": readout("°C"),
        "spice": [
            linear(-40, 125),
            "B_{ref} {node:1} {node:2} I=V({node:1},{node:2})/({R25}*exp({B}*(1/(V(q_{ref})+273.15)-0.0033540164)))",
            "R_{ref}_p {node:1} {node:2} 1G",
        ],
    }


def pt100():
    # Callendar-Van Dusen with the IEC 60751 coefficients.
    r = ("({R0}*(1+3.9083e-3*V(q_{ref})-5.775e-7*V(q_{ref})*V(q_{ref})"
         "+(1-u(V(q_{ref})))*(-4.183e-12)*(V(q_{ref})-100)*V(q_{ref})*V(q_{ref})*V(q_{ref})))")
    return {
        "type": "pt100",
        "name": "Platinum RTD PT100",
        "category": CATEGORY,
        "prefix": "RT",
        "description": "Platinum resistance thermometer (IEC 60751): 100 Ω at 0 °C, 138.5 Ω at 100 °C; set R0 to "
                       "1000 for a PT1000; no self-heating",
        "symbol": variable_resistor([{"text": "+t°", "at": [2.1, 1.2], "size": 0.6}]),
        "value_parameter": "temperature",
        "parameters": [
            number("R0", "Resistance at 0 °C", "Ohm", "100", "100 for a PT100, 1000 for a PT1000", "10", "10k"),
            quantity("temperature", "Temperature", "°C", "25", "-200", "850", "Temperature of the sensor"),
        ],
        "interactive": {"kind": "potentiometer", "parameter": "temperature"},
        "readout": readout("°C"),
        "spice": [
            linear(-200, 850),
            f"B_{{ref}} {{node:1}} {{node:2}} I=V({{node:1}},{{node:2}})/{r}",
            "R_{ref}_p {node:1} {node:2} 1G",
        ],
    }


# NIST ITS-90 type K reference functions (mV from °C).
K_POSITIVE = [-0.176004136860e-01, 0.389212049750e-01, 0.185587700320e-04, -0.994575928740e-07, 0.318409457190e-09,
              -0.560728448890e-12, 0.560750590590e-15, -0.320207200030e-18, 0.971511471520e-22, -0.121047212750e-25]
K_EXPONENTIAL = (0.118597600000e+00, -0.118343200000e-03, 0.126968600000e+03)
K_NEGATIVE = [0.0, 0.394501280250e-01, 0.236223735980e-04, -0.328589067840e-06, -0.499048287770e-08,
              -0.675090591730e-10, -0.574103274280e-12, -0.310888728940e-14, -0.104516093650e-16,
              -0.198892668780e-19, -0.163226974860e-22]


def horner(coefficients, x):
    text = repr(coefficients[-1])
    for c in reversed(coefficients[:-1]):
        text = f"{c!r}+{x}*({text})"
    return f"({text})"


def type_k_mv(x):
    a0, a1, a2 = K_EXPONENTIAL
    positive = f"({horner(K_POSITIVE, x)}+{a0!r}*exp({a1!r}*({x}-{a2!r})*({x}-{a2!r})))"
    return f"(u({x})*{positive}+(1-u({x}))*{horner(K_NEGATIVE, x)})"


def thermocouple():
    return {
        "type": "thermocouple_k",
        "name": "Thermocouple type K",
        "category": CATEGORY,
        "prefix": "TC",
        "description": "Chromel-alumel thermocouple (NIST ITS-90 tables): about 41 µV/°C between the hot junction "
                       "and the cold junction at the terminals; amplify it (an op-amp or a MAX6675-style chip)",
        "symbol": {
            "graphics": [
                {"line": [0, -3, 0, -1.5]},
                {"line": [0, 3, 0, 1.5]},
                {"line": [0, -1.5, 1.6, 0]},
                {"line": [0, 1.5, 1.6, 0]},
                {"circle": [1.6, 0, 0.25], "fill": True},
                {"text": "+", "at": [-0.8, -2.2], "size": 0.7},
                {"text": "K", "at": [-1.0, 0], "size": 0.8},
            ],
            "pins": [pin("+", 0, -3, "up"), pin("-", 0, 3, "down")],
        },
        "value_parameter": "temperature",
        "parameters": [
            quantity("temperature", "Hot junction", "°C", "25", "-200", "1250", "Temperature at the measuring tip"),
            number("Tcj", "Cold junction", "°C", "25", "Temperature of the terminals (the reference junction)",
                   "-50", "100"),
            number("R", "Wire resistance", "Ohm", "10", "Resistance of the thermocouple wires", "0.01", "1k"),
        ],
        "interactive": {"kind": "potentiometer", "parameter": "temperature"},
        "readout": readout("°C"),
        "spice": [
            linear(-200, 1250),
            "B_{ref}_c c_{ref} 0 V={Tcj}",
            f"B_{{ref}}_e e_{{ref}} {{node:-}} V=0.001*({type_k_mv('V(q_{ref})')}-{type_k_mv('V(c_{ref})')})",
            "R_{ref}_w e_{ref} {node:+} {R}",
        ],
    }


def ldr():
    return {
        "type": "ldr",
        "name": "Light-dependent resistor (LDR)",
        "category": CATEGORY,
        "prefix": "R",
        "description": "Photoresistor like the GL5528: about 10 kΩ at 10 lux, falling with more light (gamma "
                       "0.7), 1 MΩ in the dark; real LDRs take tens of milliseconds to follow the light",
        "symbol": {
            "graphics": [
                {"rect": [-0.6, -1.8, 1.2, 3.6]},
                {"line": [0, -3, 0, -1.8]},
                {"line": [0, 1.8, 0, 3]},
            ] + LIGHT_ARROWS,
            "pins": [pin("1", 0, -3, "up"), pin("2", 0, 3, "down")],
        },
        "value_parameter": "light",
        "parameters": [
            number("R10", "Resistance at 10 lux", "Ohm", "10k", "Resistance at 10 lux (GL5528: 8 to 20 kΩ)", "100",
                   "10M"),
            number("gamma", "Gamma", "", "0.7", "Slope: log(R10/R100), the fall per decade of light", "0.3", "1.2"),
            number("Rdark", "Dark resistance", "Ohm", "1M", "Resistance in the dark", "10k", "1G"),
            quantity("light", "Illuminance", "lux", "100", "0.1", "100k",
                     "Light on the sensor (office about 500 lux, daylight 10k lux and more)"),
        ],
        "interactive": {"kind": "potentiometer", "parameter": "light", "scale": "log"},
        "readout": readout("lux"),
        "spice": [
            logarithmic(0.1, 100000),
            "B_{ref} {node:1} {node:2} I=V({node:1},{node:2})*(exp({gamma}*ln(V(q_{ref})/10))/{R10}+1/{Rdark})",
        ],
    }


def bpw34():
    return {
        "type": "bpw34",
        "name": "Photodiode BPW34",
        "category": CATEGORY,
        "prefix": "D",
        "description": "Silicon PIN photodiode: about 50 µA per 1000 lux, reverse-biased (photoconductive) or into a "
                       "transimpedance amplifier; about 0.35 V open-circuit at 1000 lux",
        "symbol": {
            "graphics": [
                {"polygon": [-1, -0.8, 1, -0.8, 0, 0.8]},
                {"line": [-1, 0.8, 1, 0.8]},
                {"line": [0, -3, 0, -0.8]},
                {"line": [0, 0.8, 0, 3]},
            ] + LIGHT_ARROWS,
            "pins": [pin("A", 0, -3, "up"), pin("K", 0, 3, "down")],
        },
        "value_parameter": "light",
        "parameters": [
            quantity("light", "Illuminance", "lux", "1k", "0.1", "100k",
                     "Light on the sensor (office about 500 lux, daylight 10k lux and more)"),
        ],
        "interactive": {"kind": "potentiometer", "parameter": "light", "scale": "log"},
        "readout": readout("lux"),
        "spice": [
            logarithmic(0.1, 100000),
            "D_{ref} {node:A} {node:K} DPH_{ref}",
            "B_{ref}_p {node:K} {node:A} I=5e-8*V(q_{ref})",
            "R_{ref}_d {node:A} {node:K} 5G",
            ".model DPH_{ref} D(IS=6.2e-11 N=1 RS=10 CJO=70p M=0.33 BV=60 IBV=10u)",
        ],
    }


def acs712():
    s = supply_ok("VCC", "GND", 4)
    vcc = "V({node:VCC},{node:GND})"
    return {
        "type": "acs712",
        "name": "Current sensor ACS712",
        "category": CATEGORY,
        "prefix": "U",
        "description": "Hall-effect current sensor: VOUT = VCC/2 + sensitivity × current from IP+ to IP- "
                       "(ratiometric to a 5 V supply); the current path is 1.2 mΩ and isolated from the outputs",
        "symbol": chip_symbol("ACS712", left=[("IP+", -2), ("IP-", 2)], right=[("VOUT", 0)], top=[("VCC", 0)],
                              bottom=[("GND", 0)], width=10, height=12),
        "designator_offset": [6, -7],
        "value_offset": [6, 7],
        "value_parameter": "sensitivity",
        "parameters": [
            number("sensitivity", "Sensitivity", "V/A", "185m",
                   "185 mV/A for the ACS712-05B (±5 A), 100 mV/A for -20A, 66 mV/A for -30A", "1m", "10"),
        ],
        "readout": {"kind": "current", "element": "V_{ref}_s"},
        "spice": [
            "R_{ref}_p {node:IP+} m_{ref} 1.2m",
            "V_{ref}_s m_{ref} {node:IP-} DC 0",
            f"B_{{ref}}_o o_{{ref}} {{node:GND}} V={s}*min(max({vcc}*(0.5+{{sensitivity}}*I(V_{{ref}}_s)/5),0),"
            f"{vcc})",
            "R_{ref}_o o_{ref} {node:VOUT} 1",
            "R_{ref}_q {node:VCC} {node:GND} 500",
        ],
    }


def load_cell():
    x = "{= sensitivity / 1000}*V({control})"
    arm = "V({node:%s},{node:%s})/({R}*(1%s%s))"
    return {
        "type": "load_cell",
        "name": "Load cell",
        "category": CATEGORY,
        "prefix": "LC",
        "description": "Strain-gauge bridge: the output S+ − S- is the excitation × sensitivity × load / capacity "
                       "(1 mV/V: 5 mV at full load with 5 V); amplify it, for example with an HX711",
        "symbol": {
            "graphics": [
                {"rect": [-3, -3, 6, 6]},
                {"polygon": [-2, 0, 0, -2, 2, 0, 0, 2]},
                {"line": [-5, -2, -3, -2]}, {"text": "E+", "at": [-2.1, -2.4], "size": 0.5},
                {"line": [-5, 2, -3, 2]}, {"text": "E-", "at": [-2.1, 2.4], "size": 0.5},
                {"line": [5, -2, 3, -2]}, {"text": "S+", "at": [2.1, -2.4], "size": 0.5},
                {"line": [5, 2, 3, 2]}, {"text": "S-", "at": [2.1, 2.4], "size": 0.5},
                {"line": [0, -4.6, 0, -3.4]},
                {"polygon": [0, -3.1, -0.4, -3.8, 0.4, -3.8], "fill": True},
            ],
            "pins": [pin("E+", -5, -2, "left"), pin("E-", -5, 2, "left"), pin("S+", 5, -2, "right"),
                     pin("S-", 5, 2, "right")],
        },
        "designator_offset": [5, -5],
        "value_offset": [5, 5],
        "value_parameter": "capacity",
        "parameters": [
            number("capacity", "Capacity", "kg", "5", "Rated load", "0.1", "100k"),
            number("sensitivity", "Sensitivity", "mV/V", "1", "Output at the rated load per volt of excitation",
                   "0.1", "10"),
            number("R", "Bridge resistance", "Ohm", "1k", "Resistance of each strain gauge", "100", "10k"),
            number("load", "Load", "%", "0", "Load as a share of the capacity; drag the part or use the wheel while "
                   "running (shown in kg)", "0", "100"),
        ],
        "interactive": {"kind": "potentiometer", "parameter": "load"},
        "readout": readout("kg"),
        "spice": [
            "B_{ref}_q q_{ref} 0 V={capacity}*V({control})",
            f"B_{{ref}}_1 {{node:E+}} {{node:S+}} I={arm % ('E+', 'S+', '-', x)}",
            f"B_{{ref}}_2 {{node:S+}} {{node:E-}} I={arm % ('S+', 'E-', '+', x)}",
            f"B_{{ref}}_3 {{node:E+}} {{node:S-}} I={arm % ('E+', 'S-', '+', x)}",
            f"B_{{ref}}_4 {{node:S-}} {{node:E-}} I={arm % ('S-', 'E-', '-', x)}",
        ],
    }


def a3144():
    s = supply_ok("VCC", "GND", 3.8)
    return {
        "type": "a3144",
        "name": "Hall switch A3144",
        "category": CATEGORY,
        "prefix": "U",
        "description": "Unipolar Hall-effect switch: the open-collector OUT goes low while a south pole gives more "
                       "than the operate point, and is released below the release point; needs a pull-up",
        "symbol": chip_symbol("A3144", right=[("OUT", 0)], top=[("VCC", 0)], bottom=[("GND", 0)]),
        "designator_offset": [5, -5],
        "value_offset": [5, 5],
        "value_parameter": "field",
        "parameters": [
            quantity("field", "Magnetic field", "mT", "0", "-100", "100",
                     "Field at the sensor's face, positive for a south pole"),
            number("Bop", "Operate point", "mT", "20", "Field that switches the output on (datasheet: 7 to 35 mT)",
                   "1", "90"),
            number("Brp", "Release point", "mT", "15", "Field that releases the output (below the operate point)",
                   "0", "89"),
        ],
        "interactive": {"kind": "potentiometer", "parameter": "field"},
        "readout": readout("mT"),
        "spice": [
            linear(-100, 100),
            # A Schmitt trigger: the threshold is the operate point while off and the release point while on.
            "B_{ref}_st 0 st_{ref} I=1m*(0.5*(1+tanh(4*(V(q_{ref})-{Brp}-({Bop}-{Brp})*(1-V(st_{ref})))))-V(st_{ref}))",
            "C_{ref}_st st_{ref} 0 1n",
            "R_{ref}_st st_{ref} 0 1G",
            f"B_{{ref}}_out {{node:OUT}} {{node:GND}} I=V(st_{{ref}})*{s}*max(V({{node:OUT}},{{node:GND}}),0)/20",
            "R_{ref}_l {node:OUT} {node:GND} 100Meg",
            "R_{ref}_q {node:VCC} {node:GND} 1k",
        ],
    }


def pc817():
    return {
        "type": "pc817",
        "name": "Optocoupler PC817",
        "category": CATEGORY,
        "prefix": "U",
        "description": "Phototransistor optocoupler: the LED current from A to K lets the transistor pass up to "
                       "CTR × that current from C to E; input and output are isolated",
        "symbol": {
            "graphics": [
                {"rect": [-4, -3, 8, 6]},
                {"line": [-6, -2, -2, -2]}, {"line": [-2, -2, -2, -0.8]},
                {"polygon": [-3, -0.8, -1, -0.8, -2, 0.8]},
                {"line": [-3, 0.8, -1, 0.8]},
                {"line": [-2, 0.8, -2, 2]}, {"line": [-6, 2, -2, 2]},
                {"line": [-0.6, -0.6, 0.6, 0]},
                {"polygon": [0.9, 0.15, 0.3, 0.2, 0.55, -0.3], "fill": True},
                {"line": [-0.6, 0.4, 0.6, 1.0]},
                {"polygon": [0.9, 1.15, 0.3, 1.2, 0.55, 0.7], "fill": True},
                {"line": [1.4, -1, 1.4, 1]},
                {"line": [1.4, -0.4, 2.4, -1.4]}, {"line": [2.4, -1.4, 2.4, -2]}, {"line": [2.4, -2, 6, -2]},
                {"line": [1.4, 0.4, 2.4, 1.4]}, {"line": [2.4, 1.4, 2.4, 2]}, {"line": [2.4, 2, 6, 2]},
                {"polygon": [2.4, 1.4, 1.8, 1.25, 2.25, 0.8], "fill": True},
                {"text": "A", "at": [-3.4, -2.4], "size": 0.5}, {"text": "K", "at": [-3.4, 2.4], "size": 0.5},
                {"text": "C", "at": [3.4, -2.4], "size": 0.5}, {"text": "E", "at": [3.4, 2.4], "size": 0.5},
            ],
            "pins": [pin("A", -6, -2, "left"), pin("K", -6, 2, "left"), pin("C", 6, -2, "right"),
                     pin("E", 6, 2, "right")],
        },
        "designator_offset": [5, -5],
        "value_offset": [5, 5],
        "value_parameter": "CTR",
        "parameters": [
            number("CTR", "Current transfer ratio", "%", "100",
                   "Collector current / LED current (PC817: 50 to 600 %, by rank A to D)", "10", "600"),
        ],
        "readout": {"kind": "current", "element": "V_{ref}_led"},
        "spice": [
            "V_{ref}_led {node:A} a_{ref} DC 0",
            "D_{ref} a_{ref} {node:K} DIR_{ref}",
            "B_{ref}_c {node:C} {node:E} I={= CTR / 100}*max(I(V_{ref}_led),0)*tanh(max(V({node:C},{node:E}),0)/0.15)",
            "R_{ref}_ce {node:C} {node:E} 100Meg",
            ".model DIR_{ref} D(IS=1e-14 N=1.5 RS=3 CJO=30p BV=6 IBV=10u)",
        ],
    }


def solenoid():
    pull_in = "0.6 * V / Rcoil"
    release = "0.25 * V / Rcoil"
    return {
        "type": "solenoid",
        "name": "Solenoid",
        "category": CATEGORY,
        "prefix": "L",
        "description": "Linear solenoid: a coil whose plunger is pulled in above 60 % of the rated current and "
                       "springs out below 25 %; add a flyback diode across the coil",
        "symbol": {
            "graphics": [
                {"line": [-5, -2, -3, -2]}, {"line": [-5, 2, -3, 2]},
                {"line": [-3, -2, -3, -1.6]}, {"line": [-3, 2, -3, 1.6]},
                {"arc": [-3, -1.2, 0.4, 90, -180]},
                {"arc": [-3, -0.4, 0.4, 90, -180]},
                {"arc": [-3, 0.4, 0.4, 90, -180]},
                {"arc": [-3, 1.2, 0.4, 90, -180]},
                {"rect": [-2, -1.8, 3, 3.6]},
            ],
            "pins": [pin("1", -5, -2, "left"), pin("2", -5, 2, "left")],
            "states": {
                "0": [{"rect": [0, -0.5, 4, 1], "fill": True}, {"line": [4, -1, 4, 1]}],
                "1": [{"rect": [-1.5, -0.5, 4, 1], "fill": True}, {"line": [2.5, -1, 2.5, 1]}],
            },
        },
        "designator_offset": [-3, -3],
        "value_offset": [2, 3],
        "value_parameter": "V",
        "parameters": [
            number("V", "Rated voltage", "V", "12", "Voltage for the rated current", "1", "240"),
            number("Rcoil", "Coil resistance", "Ohm", "24", "Resistance of the coil (rated current = V / R)", "0.1",
                   "10k"),
            number("L", "Coil inductance", "H", "30m", "Inductance of the coil", "1u", "10"),
        ],
        "readout": {"kind": "current", "element": "V_sense_{ref}", "switch": {"on": pull_in, "off": release}},
        "spice": [
            "V_sense_{ref} {node:1} c1_{ref} DC 0",
            "R_{ref}_coil c1_{ref} c2_{ref} {Rcoil}",
            "L_{ref}_coil c2_{ref} {node:2} {L}",
        ],
    }


PARTS = [lm35, ntc, pt100, thermocouple, ldr, bpw34, acs712, load_cell, a3144, pc817, solenoid]


def main():
    os.makedirs(FOLDER, exist_ok=True)
    for make in PARTS:
        part = make()
        with open(os.path.join(FOLDER, part["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
            json.dump(part, fh, indent=2, ensure_ascii=False)
            fh.write("\n")
    print(f"Wrote {len(PARTS)} parts to {FOLDER}")


if __name__ == "__main__":
    main()
