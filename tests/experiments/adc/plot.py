"""Plot the E-04 ramp and conversion timing evidence."""
from __future__ import annotations

import argparse
import json
import os
import tempfile
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    cache = tempfile.TemporaryDirectory(prefix='simnodus-matplotlib-')
    os.environ.setdefault('MPLCONFIGDIR', cache.name)
    try:
        import matplotlib
        matplotlib.use('Agg')
        matplotlib.rcParams['svg.hashsalt'] = 'simnodus-e04'
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise SystemExit('Install matplotlib only for the optional evidence plot') from error
    source = json.loads(args.input.read_text(encoding='utf-8'))
    run = source['runs'][0]
    ramp = run['ramp']
    timing = run['timing']
    figure, axes = plt.subplots(2, 1, figsize=(8, 6), constrained_layout=True)
    volts = [row['input_uv'] / 1e6 for row in ramp]
    axes[0].plot(volts, [row['expected_code'] for row in ramp], linewidth=2.0, label='Declared ideal code')
    axes[0].scatter(volts, [row['actual_code'] for row in ramp], s=10, label='Firmware readback', zorder=3)
    axes[0].set(xlabel='ADC input (V)', ylabel='12-bit code', title='101-point firmware ADC ramp')
    axes[0].grid(True, alpha=0.25)
    axes[0].legend(loc='best', fontsize=8)
    settings = [row['setting'] for row in timing]
    durations = [row['completed_us'] - row['started_us'] for row in timing]
    axes[1].bar(settings, durations, color='#4472c4')
    axes[1].set_xticks(settings)
    axes[1].set(xlabel='STM32F103 sample-time setting', ylabel='Conversion duration (us)',
                title='Fixed 1 MHz experimental ADC clock')
    axes[1].grid(True, axis='y', alpha=0.25)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(args.output, metadata={'Date': '2026-09-01', 'Creator': 'SimNodus E-04'})
    if args.output.suffix.lower() == '.svg':
        lines = args.output.read_text(encoding='utf-8').splitlines()
        args.output.write_text('\n'.join(line.rstrip() for line in lines) + '\n', encoding='utf-8', newline='\n')
    cache.cleanup()


if __name__ == '__main__':
    main()
