"""Plot compact E-03 replay and sampled-delay evidence."""
from __future__ import annotations

import argparse
import csv
import json
import os
import tempfile
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('summary', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    cache = tempfile.TemporaryDirectory(prefix='simnodus-matplotlib-')
    os.environ.setdefault('MPLCONFIGDIR', cache.name)
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise SystemExit('Install matplotlib only for the optional evidence plot') from error
    with (args.run / 'replay-r1/analog.csv').open(encoding='utf-8', newline='') as stream:
        analog = [{key: float(value) for key, value in row.items()} for row in csv.DictReader(stream)]
    compact = json.loads(args.summary.read_text(encoding='utf-8'))
    figure, axes = plt.subplots(2, 1, figsize=(8, 6), constrained_layout=True)
    time_ms = [row['time_s'] * 1000 for row in analog]
    axes[0].plot(time_ms, [row['input_v'] for row in analog], label='Replayed PA0 source', linewidth=1.2)
    axes[0].plot(time_ms, [row['output_v'] for row in analog], label='RC output', linewidth=1.5)
    axes[0].axhline(2.31, color='#b35c00', linestyle='--', linewidth=0.9, label='HIGH threshold')
    axes[0].axhline(0.99, color='#6a3d9a', linestyle='--', linewidth=0.9, label='LOW threshold')
    axes[0].set(xlabel='Virtual time (ms)', ylabel='Voltage (V)', title='Known-schedule replay')
    axes[0].legend(loc='best', fontsize=8)
    quanta = [20, 100, 1000]
    maximum_delay = [compact['sampled_profiles'][str(value)]['maximum_delay_us'] for value in quanta]
    axes[1].plot(quanta, maximum_delay, marker='o', label='Measured maximum delay')
    axes[1].plot(quanta, [value + 2 for value in quanta], linestyle='--', label='Predeclared Q + 2 us limit')
    axes[1].set_xscale('log')
    axes[1].set_yscale('log')
    axes[1].set_xticks(quanta, labels=[str(value) for value in quanta])
    axes[1].grid(True, which='both', alpha=0.25)
    axes[1].set(xlabel='Exchange quantum Q (us)', ylabel='Delay (us)',
                title='Sampled feedback remains approximate')
    axes[1].legend(loc='best', fontsize=8)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(args.output, metadata={'Date': '2026-08-31', 'Creator': 'SimNodus E-03'})
    cache.cleanup()


if __name__ == '__main__':
    main()
