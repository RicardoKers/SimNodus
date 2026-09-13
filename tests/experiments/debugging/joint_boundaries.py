"""Exercise persistent ngspice boundaries and nearby rounding-sensitive times."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    binary = ROOT / 'build/sn016/persistent-native/Debug/e05_joint_circuit.exe'
    dll = ROOT / 'build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll'
    command = list(map(str, [binary, dll, ROOT / 'build/deps/ngspice-console/Spice64/bin',
                            ROOT / 'tools/backend_probe/initialization', '10000000', 'none', 'none']))
    boundaries = sorted(set(range(2012000, 4010000, 31250)) | {3139875, 3139999, 3140000, 3140001, 3140125, 4010749})
    boundaries += [3140000] * 2
    report = {'status': 'running', 'cases': [], 'complete_e05_profile': False,
              'sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (binary, dll, Path(__file__), Path(__file__).with_name('joint_circuit.cpp'))}}
    try:
        for boundary in boundaries:
            text = f'advance 2010875\ninspect\nadvance 2011375\nhigh\ninspect\nadvance {boundary}\ninspect\nadvance 4010750\ninspect\nadvance 4027000\ninspect\nquit\n'
            result = subprocess.run(command, input=text, text=True, capture_output=True, timeout=10)
            case = {'boundary_ns': boundary, 'exit': result.returncode, 'stderr': result.stderr}
            report['cases'].append(case)
            if result.returncode:
                case['stdout'] = result.stdout
                raise RuntimeError(f'Boundary {boundary} failed: {result.stderr}')
            snapshots = [json.loads(line) for line in result.stdout.splitlines()]
            expected = [0, 2010875, 2010875, 2011375, 2011375, 2011375, boundary, boundary, 4010750, 4010750, 4027000, 4027000]
            assert [p['time_ns'] for p in snapshots] == expected
            for a, b in ((1,2), (3,4), (4,5), (6,7), (8,9), (10,11)):
                assert snapshots[a] == snapshots[b], 'Inspection or GPIO change advanced state'
            for point in snapshots:
                t = point['time_ns']
                assert abs(point['actual_s'] - t*1e-9) <= 1e-12, 'Boundary tolerance exceeded'
                voltage = 0 if t <= 2011375 else 3.3*(1-math.exp(-(t-2011375)*1e-9/.001))
                assert abs(point['output_v'] - voltage) <= 1e-5, 'RC error exceeded'
            assert [snapshots[i]['samples'] for i in (0,1,3,6,8,10)] == sorted(set(snapshots[i]['samples'] for i in (0,1,3,6,8,10)))
            uv = int(math.floor(snapshots[8]['output_v']*1e6+.5))
            assert min(4095, uv*4096//3300000) == 3541, 'ADC boundary voltage changed'
            case['snapshots'] = snapshots
        report['status'] = 'passed'
    except (AssertionError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        report['status'] = 'failed'
        report['error'] = str(error)
    (args.output/'summary.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(report['status'], len(report['cases']), 'cases', report.get('error',''))
    return 0 if report['status']=='passed' else 1

if __name__ == '__main__':
    raise SystemExit(main())
