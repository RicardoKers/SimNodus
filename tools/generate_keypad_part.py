"""Writes the 4x4 matrix keypad of resources/library/switches (SN-210).

Like the usual membrane keypad: eight pins R1-R4 (rows) and C1-C4 (columns);
a key held with the mouse joins its row to its column. The keypad's control
is the number of the key held (1 to 16, 0 for none); each key's switch closes
when the control is within a quarter of its number.

Run from the repository root: python tools/generate_keypad_part.py
"""
import json
import os

FOLDER = "resources/library/switches"
LABELS = ["1", "2", "3", "A", "4", "5", "6", "B", "7", "8", "9", "C", "*", "0", "#", "D"]
KEY = 3.2      # key size in grid units
PITCH = 3.7    # key spacing
LEFT = -7.2    # first key's left edge
TOP = -16.2    # first key's top edge


def key_rect(k):
    row, column = divmod(k, 4)
    return [round(LEFT + column * PITCH, 2), round(TOP + row * PITCH, 2), KEY, KEY]


def keypad():
    pins = [f"R{i}" for i in range(1, 5)] + [f"C{i}" for i in range(1, 5)]
    graphics = [{"rect": [-8, -17, 16, 18]}]
    for k, label in enumerate(LABELS):
        x, y, w, h = key_rect(k)
        graphics.append({"rect": [x, y, w, h]})
        graphics.append({"text": label, "at": [round(x + w / 2, 2), round(y + h / 2, 2)], "size": 1.3})
    symbol_pins = []
    for i, name in enumerate(pins):
        x = -7 + 2 * i
        graphics.append({"line": [x, 1, x, 3]})
        graphics.append({"text": name, "at": [x, 0.2], "size": 0.55})
        symbol_pins.append({"name": name, "at": [x, 3], "dir": "down"})
    # A held key: an inner outline, as if pushed in.
    states = {}
    for k in range(16):
        x, y, w, h = key_rect(k)
        states[str(k + 1)] = [{"rect": [round(x + 0.35, 2), round(y + 0.35, 2), round(w - 0.7, 2), round(h - 0.7, 2)]},
                              {"rect": [round(x + 0.2, 2), round(y + 0.2, 2), round(w - 0.4, 2), round(h - 0.4, 2)]}]
    spice = []
    for k in range(16):
        row, column = divmod(k, 4)
        n = k + 1
        spice.append(f"B_{{ref}}_k{n} k{n}_{{ref}} 0 V=max(0,1-2*abs(V({{control}})-{n}))")
        spice.append(f"S_{{ref}}_{n} {{node:R{row + 1}}} {{node:C{column + 1}}} k{n}_{{ref}} 0 SW_SN")
    return {
        "type": "keypad_4x4",
        "name": "Keypad 4x4",
        "category": "Switches & Controls",
        "prefix": "KP",
        "description": "Matrix keypad (1-9, 0, A-D, * and #): a key held with the mouse joins its row to its "
                       "column; pins R1-R4 and C1-C4 as on the membrane keypad",
        "symbol": {"graphics": graphics, "states": states, "pins": symbol_pins},
        "designator_offset": [9, -16],
        "interactive": {"kind": "keypad", "keys": [key_rect(k) for k in range(16)]},
        "spice": spice,
        "models": [".model SW_SN SW(VT=0.5 VH=0.1 RON=0.01 ROFF=1e15)"],
    }


def main():
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "part_descriptions.json"), encoding="utf-8") as fh:
        short = json.load(fh)
    p = keypad()
    p["description"] = short.get(p["type"], p["description"])
    with open(os.path.join(FOLDER, p["type"] + ".json"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write(json.dumps(p, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
