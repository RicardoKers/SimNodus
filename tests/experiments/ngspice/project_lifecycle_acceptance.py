"""Explicit real-engine acceptance after native project persistence round trip."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tests/schema'))
import native_lifecycle_regression as lifecycle
import native_rc_compile_regression as compiler
import run as e01


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args();output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {'status': 'started', 'voltage_tolerance_v': e01.VOLTAGE_TOLERANCE,
              'time_tolerance_s': e01.TIME_TOLERANCE}
    lifecycle.PROBE = str(ROOT / 'build/sn021-save/Debug/native_lifecycle_probe.exe')
    compiler.PROBE = str(ROOT / 'build/sn021-save/Debug/native_rc_compile_probe.exe')
    try:
        with (output / 'assets.log').open('w', encoding='utf-8') as log:
            subprocess.run([sys.executable, str(ROOT / 'tools/check_backend_assets.py'), '--ngspice-only'],
                           cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        package = output / 'package';doc = compiler.project();compiler.populate(package, doc)
        raw = (json.dumps(doc, indent=2) + '\n').encode()
        result = lifecycle.lifecycle(package, raw)
        assert 'error' not in result, result
        assert (package / 'original.json').read_bytes() == raw
        assert (package / 'revised.json').read_bytes() == result['revised']
        # Compare independently compiled pre-save input with the reopened artifact.
        baseline = compiler.compile_project(doc, package)
        assert isinstance(baseline, dict), baseline
        assert result['netlist'] == baseline['netlist']
        report['netlist_matches_pre_save_compilation'] = True
        report['original_preserved'] = True
        (output / 'rc.cir').write_bytes(result['netlist'])
        deps = ROOT / 'build/deps'
        argv = [str(ROOT / 'build/e01/Debug/e01.exe'), str(deps / 'ngspice/Spice64_dll/dll-vs/ngspice.dll'),
                str(deps / 'ngspice-console/Spice64/bin'), str(ROOT / 'tools/backend_probe/initialization'),
                str(output), 'rc', str(output)]
        report['command'] = [v.replace(str(ROOT), '<repository>') for v in argv]
        with (output / 'engine.log').open('w', encoding='utf-8') as log:
            process = subprocess.run(argv, cwd=output, stdout=log, stderr=subprocess.STDOUT, timeout=30)
        report['process_exit'] = process.returncode;assert process.returncode == 0
        metrics = json.loads((output / 'metrics.json').read_text(encoding='utf-8'));report['metrics'] = metrics
        assert metrics['idle_before_quit'] == 1 and metrics['quit_requested'] == 1 and metrics['exit_status'] == 0
        assert metrics['callback_fault'] == 0
        report['analytical'] = e01.analyze(output / 'first.csv', False, 0.005)
        report['project_voltage_tolerance_v'] = doc['temporal']['voltage_tolerance_uv'] * 1e-6
        assert report['analytical']['max_error_v'] <= report['project_voltage_tolerance_v']
        callbacks, vectors = e01.rows(output / 'callbacks.csv'), e01.rows(output / 'first.csv')
        assert len(callbacks) == len(vectors)
        assert all(all(a[k] == b[k] for k in ('time_s', 'input_v', 'output_v')) for a, b in zip(callbacks, vectors))
        report['callbacks_match_vectors'] = True;report['status'] = 'passed'
    except Exception as error:
        report['status'] = 'failed';report['error'] = str(error);raise
    finally:
        report['files_sha256'] = {p.relative_to(output).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in sorted(output.rglob('*')) if p.is_file()}
        report['executables_sha256'] = {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [Path(lifecycle.PROBE), Path(compiler.PROBE), ROOT / 'build/e01/Debug/e01.exe'] if p.is_file()}
        (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
