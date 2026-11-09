"""Writes the lithium batteries of resources/library/energy (SN-275).

Thevenin model of Ns equal cells in series, for chargers and battery-powered
circuits:

    + ── V_sense ── R0 ── (R1 || C1) ── OCV(SOC) ── -       (all times Ns)

The state of charge is the voltage of node s_<ref>: SOC0 plus the charge
counted on a 1 F capacitor, dQ/dt = I / Qeff, where I is the current into the
+ pin and Qeff = capacity x 3.6 C/mAh / acceleration. Interactive runs carry
the capacitor's voltage from segment to segment. The open-circuit voltage per
cell comes from a table of the chemistry (NMC/LCO 3.0-4.2 V, LiFePO4
2.5-3.65 V; typical slow-discharge curves at 25 °C); above 100 % and below 0 %
it rises and falls steeply, so an overcharge shows.

The time acceleration divides the capacity and the RC time constant: a CC/CV
charge of hours runs in minutes, with the right voltages and currents.

The optional protection, like a DW01 with two MOSFETs: charging stops above
the overcharge voltage per cell until the cell falls back, discharging stops
below the over-discharge voltage until it recovers; each switch has a diode
that still lets the other direction through.

Not modeled: temperature, aging, self-discharge, hysteresis of the OCV, the
current dependence of the capacity.

Run from the repository root: python tools/generate_battery_parts.py
"""
import json
import os

FOLDER = "resources/library/energy"
CATEGORY = "Sources"

CHEMISTRIES = [
    {
        "type": "li_ion_battery",
        "tag": "LIION",
        "name": "Lithium-ion battery",
        "what": "Lithium-ion (NMC/LCO) cells, 3.7 V nominal, 4.2 V full, 3.0 V empty",
        "capacity": "2500",
        "r0": "50m",
        "r1": "30m",
        "tau": "30",
        # SOC (0..1) -> open-circuit volts per cell.
        "ocv": [(0, 3.0), (0.05, 3.45), (0.1, 3.55), (0.2, 3.62), (0.3, 3.68), (0.4, 3.73), (0.5, 3.78),
                (0.6, 3.85), (0.7, 3.92), (0.8, 4.0), (0.9, 4.08), (1.0, 4.2)],
        "over": (4.25, 4.15),      # overcharge: off above, back on below (per cell)
        "under": (2.5, 3.0),       # over-discharge: off below, back on above
    },
    {
        "type": "lifepo4_battery",
        "tag": "LFP",
        "name": "LiFePO4 battery",
        "what": "Lithium iron phosphate cells, 3.2 V nominal, 3.65 V full, 2.5 V empty",
        "capacity": "1500",
        "r0": "40m",
        "r1": "20m",
        "tau": "30",
        "ocv": [(0, 2.5), (0.05, 3.0), (0.1, 3.15), (0.2, 3.22), (0.3, 3.25), (0.4, 3.27), (0.5, 3.28),
                (0.6, 3.29), (0.7, 3.3), (0.8, 3.32), (0.9, 3.35), (0.95, 3.4), (1.0, 3.6)],
        "over": (3.75, 3.6),
        "under": (2.0, 2.5),
    },
]


def number(name, label, unit, default, description, minimum=None, maximum=None):
    p = {"name": name, "label": label, "unit": unit, "default": default, "description": description}
    if minimum is not None:
        p["min"] = minimum
    if maximum is not None:
        p["max"] = maximum
    return p


def choice(name, label, default, choices, description):
    return {"name": name, "label": label, "unit": "", "default": default, "choices": choices,
            "description": description}


def symbol():
    graphics = [
        {"line": [-1.6, -1.6, 1.6, -1.6]},
        {"line": [-0.8, -0.9, 0.8, -0.9]},
        {"line": [-1.6, 0.2, 1.6, 0.2]},
        {"line": [-0.8, 0.9, 0.8, 0.9]},
        {"line": [1.4, -2.6, 1.4, -2.0]},
        {"line": [1.1, -2.3, 1.7, -2.3]},
        {"line": [0, -4, 0, -1.6]},
        {"line": [0, 0.9, 0, 4]},
        {"text": "Li", "at": [-2.6, -0.4], "size": 1.1},
    ]
    pins = [{"name": "+", "at": [0, -4], "dir": "up"}, {"name": "-", "at": [0, 4], "dir": "down"}]
    return {"graphics": graphics, "pins": pins}


def spice(chemistry, protected):
    table = ",".join(f"{soc},{volts}" for soc, volts in chemistry["ocv"])
    tag = chemistry["tag"]
    lines = [
        # Charge counter: the capacitor's voltage is the charge taken in, as a share of Qeff.
        "V_{ref}_i {node:+} a_{ref} DC 0",
        "B_{ref}_q 0 q_{ref} I=I(V_{ref}_i)/({capacity}*3.6/{accel})",
        "C_{ref}_q q_{ref} 0 1",
        "R_{ref}_q q_{ref} 0 1T",
        "B_{ref}_s s_{ref} 0 V={soc0}/100+V(q_{ref})",
        # Open-circuit voltage of the pack; steep beyond full and empty.
        f"B_{{ref}}_e e_{{ref}} {{node:-}} V={{cells}}*(pwl(V(s_{{ref}}),{table})"
        f"+5*max(V(s_{{ref}})-1,0)-5*max(-V(s_{{ref}}),0))",
        "R_{ref}_1 c_{ref} e_{ref} {= cells * R1}",
        "C_{ref}_1 c_{ref} e_{ref} {= tau / accel / R1 / cells}",
        # The readout: state of charge in %.
        "B_{ref}_r r_{ref} 0 V=100*V(s_{ref})",
    ]
    if not protected:
        lines.append("R_{ref}_0 a_{ref} c_{ref} {= cells * R0}")
        return lines
    over_off, over_on = chemistry["over"]
    under_off, under_on = chemistry["under"]
    # The cell voltage the protection watches: the pack's voltage inside R0, per cell.
    lines += [
        "R_{ref}_0 a_{ref} b_{ref} {= cells * R0}",
        "B_{ref}_v v_{ref} 0 V=V(c_{ref},{node:-})/{cells}",
        # Charge switch: open above over_off, closed again below over_on; its
        # diode lets the battery discharge while it is open.
        f"B_{{ref}}_oc oc_{{ref}} 0 V={over_off}-V(v_{{ref}})",
        f"S_{{ref}}_c b_{{ref}} m_{{ref}} oc_{{ref}} 0 SW_{tag}C",
        "D_{ref}_c m_{ref} b_{ref} D_LIP",
        # Discharge switch: open below under_off, closed again above under_on.
        f"B_{{ref}}_od od_{{ref}} 0 V=V(v_{{ref}})-{under_off}",
        f"S_{{ref}}_d m_{{ref}} c_{{ref}} od_{{ref}} 0 SW_{tag}D",
        "D_{ref}_d m_{ref} c_{ref} D_LIP",
    ]
    return lines


def models(chemistry):
    """The diode and the two switches with hysteresis of the protection."""
    over_off, over_on = chemistry["over"]
    under_off, under_on = chemistry["under"]
    tag = chemistry["tag"]
    half_over = (over_off - over_on) / 2
    half_under = (under_on - under_off) / 2
    return [
        ".model D_LIP D(IS=1e-9 N=1.2 RS=0.01)",
        f".model SW_{tag}C SW(VT={half_over:.4g} VH={half_over:.4g} RON=0.01 ROFF=1e9)",
        f".model SW_{tag}D SW(VT={half_under:.4g} VH={half_under:.4g} RON=0.01 ROFF=1e9)",
    ]


def part(chemistry, protected):
    kind = chemistry["type"] + ("_protected" if protected else "")
    name = chemistry["name"] + (" with protection" if protected else "")
    over_off, over_on = chemistry["over"]
    under_off, under_on = chemistry["under"]
    description = (f"{chemistry['what']}; Thevenin model (open-circuit voltage from the state of charge, series "
                   "resistance and an RC pair) with a time acceleration for chargers; shows the state of charge. "
                   "No temperature, aging or self-discharge")
    if protected:
        description += (f". Protection like a DW01: charging stops above {over_off} V per cell (again below "
                        f"{over_on} V), discharging below {under_off} V (again above {under_on} V)")
    parameters = [
        number("capacity", "Capacity", "mAh", chemistry["capacity"], "Capacity of each cell", "1", "100k"),
        number("cells", "Cells in series", "", "1", "Cells in series (1S to 4S); voltages and resistances add",
               "1", "4"),
        number("soc0", "Initial state of charge", "%", "50", "State of charge when the run starts", "0", "100"),
        number("accel", "Time acceleration", "", "1",
               "Divides the capacity and the RC time constant, so a charge of hours runs in minutes with the "
               "same voltages and currents (1 = real time; 60 = an hour in a minute)", "1", "3600"),
        number("R0", "Series resistance", "Ohm", chemistry["r0"], "Internal resistance of each cell", "1m", "10"),
        number("R1", "Polarization resistance", "Ohm", chemistry["r1"],
               "Resistance of the RC pair of each cell (the slow part of the voltage drop)", "1m", "10"),
        number("tau", "Polarization time constant", "s", chemistry["tau"],
               "Time constant of the RC pair (before the acceleration)", "1", "1000"),
    ]
    return {
        "type": kind,
        "name": name,
        "category": CATEGORY,
        "prefix": "BT",
        "description": description,
        "symbol": symbol(),
        "designator_offset": [3, -2],
        "value_offset": [3, 2],
        "value_parameter": "capacity",
        "parameters": parameters,
        "models": models(chemistry) if protected else [],
        "readout": {"kind": "node", "element": "r_{ref}", "scale": 1, "unit": "% SOC"},
        # Delivered current: negative while charging.
        "live": [
            {"label": "I", "kind": "current", "element": "V_{ref}_i", "scale": -1, "unit": "A"},
            {"label": "State of charge", "kind": "node", "element": "r_{ref}", "unit": "%"},
        ],
        "spice": spice(chemistry, protected),
    }


def main():
    os.makedirs(FOLDER, exist_ok=True)
    for chemistry in CHEMISTRIES:
        for protected in (False, True):
            definition = part(chemistry, protected)
            if not definition["models"]:
                del definition["models"]
            path = os.path.join(FOLDER, definition["type"] + ".json")
            with open(path, "w", encoding="utf-8", newline="\n") as fh:
                json.dump(definition, fh, indent=2, ensure_ascii=False)
                fh.write("\n")
            print(path)


if __name__ == "__main__":
    main()
