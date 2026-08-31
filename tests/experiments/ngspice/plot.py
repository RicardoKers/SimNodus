"""Optionally render the measured E-01 vectors with matplotlib (not needed for tests)."""
import argparse
import csv
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("results", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10,
                     "svg.hashsalt": "simnodus-e01", "svg.fonttype": "none"})
fig, axes = plt.subplots(2, 2, figsize=(10, 6), sharex="col", height_ratios=[2, 1], layout="constrained")
for column, case in enumerate(("rc", "external")):
    with (args.results / case / "first.csv").open(newline="", encoding="utf-8") as stream:
        data = [{key: float(value) for key, value in row.items()} for row in csv.DictReader(stream)]
    time = [row["time_s"] for row in data]
    def step(t, delay):
        return -math.expm1(-(t-delay)/0.001) if t >= delay else 0.0
    expected = [3.3 * (step(t, 0.001)-step(t, 0.003)) if case == "external" else 3.3 * step(t, 0) for t in time]
    values = [row["output_v"] for row in data]
    axes[0, column].plot([t*1000 for t in time], values, color="#12638d", linewidth=2, label="ngspice 47")
    axes[0, column].plot([t*1000 for t in time], expected, color="#d88918", linestyle="--", linewidth=1.4, label="Analytical RC")
    axes[0, column].set_title("Step at t = 0" if case == "rc" else "External pulse: 1–3 ms")
    axes[0, column].set_ylabel("Capacitor voltage (V)")
    axes[0, column].legend(frameon=False, loc="lower right")
    scale = 1e6 if case == "rc" else 1e3
    axes[1, column].plot([t*1000 for t in time], [(v-e)*scale for v, e in zip(values, expected)], color="#12638d", linewidth=1)
    axes[1, column].set_ylabel("Error (µV)" if case == "rc" else "Error (mV)")
    axes[1, column].set_xlabel("Virtual time (ms)")
    for row in axes:
        row[column].grid(alpha=0.18)
        row[column].spines[["top", "right"]].set_visible(False)
fig.suptitle("SimNodus · E-01 standalone ngspice\nR = 1 kΩ · C = 1 µF · Zero initial capacitor voltage", fontsize=13)
args.output.parent.mkdir(parents=True, exist_ok=True)
fig.savefig(args.output, metadata={"Date": None})
if args.output.suffix.lower() == ".svg":
    # Keep SVG text searchable; normalize exporter whitespace for repository hygiene.
    content = args.output.read_text(encoding="utf-8")
    args.output.write_text("\n".join(line.rstrip() for line in content.splitlines()) + "\n",
                           encoding="utf-8", newline="\n")
plt.close(fig)
