"""Explicit owned-fixture acceptance, never invoked by project opening or CTest."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tests/schema'))
sys.path.insert(0, str(ROOT / 'tests/resources'))
import native_regression as resources
import native_passive_regression as passive
import run as e01


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {'status': 'started', 'voltage_tolerance_v': 0.0165, 'time_tolerance_s': 1e-12}
    try:
        with (output / 'assets.log').open('w', encoding='utf-8') as log:
            subprocess.run([sys.executable, str(ROOT / 'tools/check_backend_assets.py'),
                            '--ngspice-only'], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        document = json.loads((ROOT / 'tests/schema/fixtures/two-rc-resource-links.json').read_text())['lock']
        resources.PROBE = str(ROOT / 'build/sn021-save/Debug/native_resource_probe.exe')
        captured = resources.native(document, str(ROOT))
        source = captured[('owned-link-fixture', 'model')].data
        passive.PROBE = str(ROOT / 'build/sn021-save/Debug/native_passive_probe.exe')
        models = passive.inspect(source)
        assert models is not None and [m[:6] for m in models] == [
            ['declared_resistor', 'first', 'second', 'value', '1000', 'R'],
            ['declared_capacitor', 'first', 'second', 'value', '1u', 'C']]
        report['captured_source_sha256'] = hashlib.sha256(source).hexdigest()
        report['models'] = models
        netlist = b'SimNodus explicit owned passive source acceptance\n' + source + b'\n' + (
            'Vdrive in 0 3.3\nXr in out declared_resistor value=1000\n'
            'Xc out 0 declared_capacitor value=1u\n.ic v(out)=0\n'
            '.options reltol=1e-6 abstol=1e-12 vntol=1e-9 method=trap maxord=2\n'
            '.save v(in) v(out)\n.tran 1u 5m 0 1u uic\n.end\n').encode()
        (output / 'rc.cir').write_bytes(netlist)
        deps = ROOT / 'build/deps'
        argv = [str(ROOT / 'build/e01/Debug/e01.exe'),
                str(deps / 'ngspice/Spice64_dll/dll-vs/ngspice.dll'),
                str(deps / 'ngspice-console/Spice64/bin'),
                str(ROOT / 'tools/backend_probe/initialization'), str(output), 'rc', str(output)]
        report['command'] = [v.replace(str(ROOT), '<repository>') for v in argv]
        with (output / 'engine.log').open('w', encoding='utf-8') as log:
            process = subprocess.run(argv, cwd=output, stdout=log, stderr=subprocess.STDOUT, timeout=30)
        report['process_exit'] = process.returncode
        assert process.returncode == 0
        metrics = json.loads((output / 'metrics.json').read_text())
        report['metrics'] = metrics
        assert metrics['idle_before_quit'] == 1 and metrics['quit_requested'] == 1 and metrics['exit_status'] == 0
        assert metrics['callback_fault'] == 0
        report['analytical'] = e01.analyze(output / 'first.csv', False, 0.005)
        callbacks, vectors = e01.rows(output / 'callbacks.csv'), e01.rows(output / 'first.csv')
        assert len(callbacks) == len(vectors)
        assert all(all(a[key] == b[key] for key in ('time_s', 'input_v', 'output_v'))
                   for a, b in zip(callbacks, vectors))
        report['callbacks_match_vectors'] = True
        report['status'] = 'passed'
    except Exception as error:
        report['status'] = 'failed'
        report['error'] = str(error)
        raise
    finally:
        report['files_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in sorted(output.iterdir()) if p.is_file()}
        (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
