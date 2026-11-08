"""Writes the photovoltaic modules of resources/library/energy (SN-274).

Single-diode model of a module of Ns cells in series:

    I = Iph - I0 (exp((V + I Rs) / a) - 1) - (V + I Rs) / Rsh,   a = n Ns k T / q

fitted from the datasheet values at STC (1000 W/m², 25 °C): Voc, Isc, Vmp,
Imp. Rs comes out explicitly (Rsh, large, is left out of these formulas):

    I0 = Isc / (exp(Voc / a) - 1)
    Rs = (Voc + a ln(1 - Imp / Isc) - Vmp) / Imp

so without Rsh the curve passes through (0, Isc), (Voc, 0) and (Vmp, Imp)
for any ideality factor n; Rsh (a leak of about 0.5 % of Imp at Vmp in the
presets) lowers the currents slightly. This script picks n for each preset so
that the maximum power point of the whole model, Rsh included, falls at Vmp
(bisection on the model's own curve); n is a parameter, so an edited
datasheet keeps its points within about 1 %, with the maximum near Vmp. The netlist keeps Rs at least 1 mΩ.

Irradiance G and cell temperature T are live controls (drag and wheel, Shift
with the wheel for T): Iph = Isc (G / 1000) (1 + alpha (T - 25)),
Voc(T) = Voc (1 + beta (T - 25)), I0(T) = Isc(T) / (exp(Voc(T) / a(T)) - 1).

The readout is the operating point's power as a share of the maximum power at
the present irradiance and temperature. That maximum comes from a second copy
of the model in the netlist: written in the diode voltage vd, the current
I = Iph - I0 (exp(vd/a) - 1) - vd/Rsh and the voltage V = vd - I Rs are
explicit, and dP/dV = 0 is I - V g / (1 + Rs g) = 0 with
g = I0/a exp(vd/a) + 1/Rsh; a node whose only element forces that function
to zero lets ngspice find the maximum (a capacitor on that node makes it a
smooth climb in a transient, which starts from 0 V).

Run from the repository root: python tools/generate_solar_parts.py
"""
import json
import math
import os

FOLDER = "resources/library/energy"
CATEGORY = "Sources"
K_Q = 8.617333262e-5  # Boltzmann constant / electron charge, V/K

PRESETS = [
    # type, name, Voc, Isc, Vmp, Imp, cells, Rsh
    ("solar_1w_6v", "Solar panel 1 W 6 V", 7.0, 0.19, 5.6, 0.179, 12, 6200.0),
    ("solar_5w_6v", "Solar panel 5 W 6 V", 7.2, 0.95, 5.7, 0.877, 12, 1300.0),
    ("solar_10w_12v", "Solar panel 10 W 12 V", 21.6, 0.62, 17.5, 0.572, 36, 6100.0),
    ("solar_20w_12v", "Solar panel 20 W 12 V", 21.6, 1.23, 17.6, 1.14, 36, 3100.0),
]
ALPHA = 0.05  # Isc, % per °C
BETA = -0.32  # Voc, % per °C


def model_curve(voc, isc, vmp, imp, cells, n, rsh=math.inf):
    """(V, I) points of the fitted model at STC, from the diode voltage."""
    a = n * cells * K_Q * 298.15
    i0 = isc / (math.exp(voc / a) - 1)
    rs = (voc + a * math.log(1 - imp / isc) - vmp) / imp
    points = []
    steps = 4000
    for k in range(steps + 1):
        vd = (voc + isc * max(rs, 0)) * 1.05 * k / steps
        i = isc - i0 * (math.exp(vd / a) - 1) - vd / rsh
        v = vd - i * rs
        if i >= 0:
            points.append((v, i))
    return points, rs


def fit_n(voc, isc, vmp, imp, cells, rsh):
    """The ideality factor that puts the model's maximum power point at Vmp."""
    def vmp_of(n):
        points, rs = model_curve(voc, isc, vmp, imp, cells, n, rsh)
        return max(points, key=lambda p: p[0] * p[1])[0], rs

    # The largest n that keeps Rs positive.
    a_max = (voc - vmp) / -math.log(1 - imp / isc)
    low, high = 0.5, min(3.0, a_max / (cells * K_Q * 298.15))
    for _ in range(60):
        middle = (low + high) / 2
        at, rs = vmp_of(middle)
        # A larger n (and so a smaller Rs) moves the maximum to higher voltages.
        if at < vmp:
            low = middle
        else:
            high = middle
    n = (low + high) / 2
    at, rs = vmp_of(n)
    if rs < 0 or abs(at - vmp) > 0.01 * vmp:
        raise SystemExit(f"cannot put the maximum at Vmp={vmp} (model {at:.3f} V, Rs {rs:.3f})")
    return n


def number(name, label, unit, default, description, minimum=None, maximum=None):
    p = {"name": name, "label": label, "unit": unit, "default": default, "description": description}
    if minimum is not None:
        p["min"] = minimum
    if maximum is not None:
        p["max"] = maximum
    return p


def fmt(value, digits=4):
    return f"{value:.{digits}g}"


def spice():
    # Runtime quantities on internal nodes (volts = their value): irradiance,
    # cell temperature, thermal voltage times n Ns, photocurrent, saturation
    # current and series resistance.
    def node(name, expression):
        return f"B_{{ref}}_{name} {name}_{{ref}} 0 V={expression}"

    def v(name):
        # Helper nodes start at 0 V in Newton's first iteration: a and Rs are
        # read with a floor so nothing divides by zero.
        if name in ("a", "rs"):
            return f"max(V({name}_{{ref}}),1e-3)"
        return f"V({name}_{{ref}})"

    def diode(x):
        # exp with a straight continuation above 60, for convergence.
        return f"(exp(min({x},60))*(1+max({x}-60,0))-1)"

    lines = [
        node("g", "1200*V({control})"),
        node("t", "-20+(85-(-20))*V({control:temperature})"),
        node("a", f"{{n}}*{{cells}}*{K_Q}*({v('t')}+273.15)"),
        node("isc", f"{{Isc}}*(1+{{alpha}}/100*({v('t')}-25))"),
        node("iph", f"{v('isc')}*{v('g')}/1000"),
        node("i0", f"{v('isc')}/(exp(min({{Voc}}*(1+{{beta}}/100*({v('t')}-25))/{v('a')},80))-1)"),
        node("rs", f"max(({{Voc}}+{{n}}*{{cells}}*{K_Q}*298.15*ln(max(1-{{Imp}}/{{Isc}},1e-6))-{{Vmp}})/{{Imp}},1e-3)"),
        # The cells: photocurrent, diode, shunt and series resistances.
        f"B_{{ref}}_ph {{node:-}} d_{{ref}} I={v('iph')}",
        f"B_{{ref}}_d d_{{ref}} {{node:-}} I={v('i0')}*{diode(f'V(d_{{ref}},{{node:-}})/' + v('a'))}",
        "R_{ref}_sh d_{ref} {node:-} {Rsh}",
        f"B_{{ref}}_s d_{{ref}} s_{{ref}} I=V(d_{{ref}},s_{{ref}})/{v('rs')}",
        # Measures the current out of the + pin.
        "V_{ref}_i s_{ref} {node:+} DC 0",
        # The shadow copy: node m_<ref> settles at the diode voltage of the
        # maximum power point, where dP/dV = I - V g / (1 + Rs g) = 0.
        node("im", f"{v('iph')}-{v('i0')}*{diode(f'V(m_{{ref}})/' + v('a'))}-V(m_{{ref}})/{{Rsh}}"),
        node("vm", f"V(m_{{ref}})-{v('im')}*{v('rs')}"),
        node("gm", f"{v('i0')}/{v('a')}*exp(min(V(m_{{ref}})/{v('a')},60))+1/{{Rsh}}"),
        f"B_{{ref}}_m 0 m_{{ref}} I={v('im')}-{v('vm')}*{v('gm')}/(1+{v('rs')}*{v('gm')})",
        "R_{ref}_m m_{ref} 0 1G",
        # In a transient the node climbs to that point instead of jumping there:
        # a run starts from 0 V, and Newton's first steps would leave the diode's
        # range. Time constant about 0.1 ms; a few ms to settle from 0 V.
        "C_{ref}_m m_{ref} 0 {= Isc / Voc * 0.003}",
        # Readout: the power taken as a share of the maximum, in %.
        node("r", f"100*max(V({{node:+}},{{node:-}})*I(V_{{ref}}_i),0)/max({v('vm')}*{v('im')},1e-9)"),
    ]
    return lines


def symbol():
    graphics = [
        {"rect": [-3, -3, 6, 6]},
        {"line": [-3, -1, 3, -1]},
        {"line": [-3, 1, 3, 1]},
        {"line": [-1, -3, -1, 3]},
        {"line": [1, -3, 1, 3]},
        {"line": [0, -4, 0, -3]},
        {"line": [0, 3, 0, 4]},
        {"text": "+", "at": [0.9, -3.6], "size": 1},
        # Light falling on the panel.
        {"line": [-5.6, -5.6, -3.6, -3.6]},
        {"line": [-3.6, -3.6, -4.4, -3.7]},
        {"line": [-3.6, -3.6, -3.7, -4.4]},
        {"line": [-4.6, -6.4, -2.6, -4.4]},
        {"line": [-2.6, -4.4, -3.4, -4.5]},
        {"line": [-2.6, -4.4, -2.7, -5.2]},
    ]
    pins = [{"name": "+", "at": [0, -4], "dir": "up"}, {"name": "-", "at": [0, 4], "dir": "down"}]
    return {"graphics": graphics, "pins": pins}


def part(kind, name, voc, isc, vmp, imp, cells, rsh):
    n = fit_n(voc, isc, vmp, imp, cells, rsh)
    points, rs = model_curve(voc, isc, vmp, imp, cells, n, rsh)
    pmax = max(v * i for v, i in points)
    description = (f"Photovoltaic module, {cells} cells in series, about {fmt(pmax, 3)} W at 1000 W/m² and 25 °C "
                   f"(Voc {fmt(voc)} V, Isc {fmt(isc)} A, Vmp {fmt(vmp)} V, Imp {fmt(imp)} A); single-diode model "
                   "fitted from these values. Irradiance and cell temperature change while running; the symbol shows "
                   "the power taken as a share of the maximum power point. No partial shading or bypass diodes")
    return {
        "type": kind,
        "name": name,
        "category": CATEGORY,
        "prefix": "PV",
        "description": description,
        "symbol": symbol(),
        "designator_offset": [4, -2],
        "value_offset": [4, 2],
        "value_parameter": "irradiance",
        "parameters": [
            number("irradiance", "Irradiance", "W/m²", "1000",
                   "Light on the panel (full sun 1000 W/m²); drag the part or use the wheel while running (0 to 1200 W/m²)",
                   "0", "1200"),
            number("temperature", "Cell temperature", "°C", "25",
                   "Temperature of the cells (25 °C at STC; about 45-60 °C in the sun); Shift with the wheel "
                   "while running (-20 to 85 °C)", "-20", "85"),
            number("Voc", "Open-circuit voltage", "V", fmt(voc), "Voc at STC, from the datasheet", "0.3", "100"),
            number("Isc", "Short-circuit current", "A", fmt(isc), "Isc at STC, from the datasheet", "1m", "20"),
            number("Vmp", "Voltage at maximum power", "V", fmt(vmp), "Vmp at STC, below Voc", "0.2", "100"),
            number("Imp", "Current at maximum power", "A", fmt(imp), "Imp at STC, below Isc", "1m", "20"),
            number("cells", "Cells in series", "", str(cells), "Number of cells in series (Ns)", "1", "200"),
            number("alpha", "Isc temperature coefficient", "%/°C", fmt(ALPHA), "Change of Isc per °C", "-1", "1"),
            number("beta", "Voc temperature coefficient", "%/°C", fmt(BETA), "Change of Voc per °C", "-1", "0"),
            number("n", "Ideality factor", "", fmt(n, 4),
                   "Diode ideality factor; fitted so the maximum power is at Vmp for the preset's values", "0.5", "3"),
            number("Rsh", "Shunt resistance", "Ohm", fmt(rsh), "Leakage across the cells (left out of the fit; about 0.5 % of Imp at Vmp)", "1", "1G"),
        ],
        "interactive": {
            "kind": "potentiometer",
            "parameter": "irradiance",
            "controls": [{"name": "temperature", "kind": "potentiometer", "parameter": "temperature"}],
        },
        "readout": {"kind": "node", "element": "r_{ref}", "scale": 1, "unit": "% MPP"},
        "spice": spice(),
    }


def main():
    os.makedirs(FOLDER, exist_ok=True)
    for preset in PRESETS:
        definition = part(*preset)
        path = os.path.join(FOLDER, preset[0] + ".json")
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(definition, fh, indent=2, ensure_ascii=False)
            fh.write("\n")
        n = next(p["default"] for p in definition["parameters"] if p["name"] == "n")
        print(f"{path}: n = {n}")


if __name__ == "__main__":
    main()
