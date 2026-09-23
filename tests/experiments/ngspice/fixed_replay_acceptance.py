"""Explicit fixed configured replay through the unchanged real E-01 host."""
import argparse
from contextlib import contextmanager, ExitStack
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tests/schema'))
import native_replay_regression as replay
compiler = replay.rc
sys.path.insert(0, str(ROOT / "tests/resources"))
import local_resources as physical
import run as e01


@contextmanager
def pinned_netlist(root, expected):
    """Test-only stage lease; containment is handle-relative, not string-based."""
    fs = physical.WindowsFiles();drive, parts = physical.root_parts(str(root))
    with ExitStack() as stack:
        current = fs.volume(drive, stack)
        for part in parts:current, _ = fs.child(current, part, True, stack, '$stage')
        handle, info = fs.child(current, 'rc.cir', False, stack, 'rc.cir')
        assert info.size() == len(expected)
        data = bytearray()
        while len(data) <= len(expected):
            chunk = fs.read(handle, len(expected) + 1 - len(data), 'rc.cir')
            if not chunk:break
            data.extend(chunk)
        assert bytes(data) == expected
        final = fs.k.GetFinalPathNameByHandleW
        final.argtypes = [wintypes.HANDLE, wintypes.LPWSTR, wintypes.DWORD, wintypes.DWORD]
        final.restype = wintypes.DWORD
        buffer = ctypes.create_unicode_buffer(32768)
        count = final(current, buffer, len(buffer), 1)
        assert 0 < count < len(buffer)
        yield Path(buffer.value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args();output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {'status': 'started', 'voltage_tolerance_v': e01.VOLTAGE_TOLERANCE,
              'time_tolerance_s': e01.TIME_TOLERANCE}
    try:
        with (output / 'assets.log').open('w', encoding='utf-8') as log:
            subprocess.run([sys.executable, str(ROOT / 'tools/check_backend_assets.py'), '--ngspice-only'],
                           cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        package = output / 'package';doc = replay.package(package)
        (output / 'project.json').write_text(json.dumps(doc, indent=2) + '\n', encoding='utf-8')
        compiler.PROBE = str(ROOT / 'build/sn021-save/Debug/native_rc_compile_probe.exe')
        compiled = replay.compile(doc, package)
        assert isinstance(compiled, dict), compiled
        report['elements'] = compiled['elements'];report['nodes'] = compiled['nodes']
        report['replay'] = compiled['replay'];assert compiled['replay'][:2] == [5000000, 5000000]
        stage = output / 'stage';stage.mkdir()
        (stage / 'rc.cir').write_bytes(compiled['netlist'])
        # Only the copied test package is changed; the historical fixture is intact.
        (package / 'tests/schema/fixtures/assets/passive.cir').write_bytes(b'* replaced after compilation\n')
        (package / 'fixed-drive.csv').write_bytes(b'replaced schedule')
        rejected = replay.compile(doc, package)
        assert isinstance(rejected, list) and rejected[0] == '2', rejected
        report['recompile_after_replacement'] = rejected
        deps = ROOT / 'build/deps'
        argv = [str(ROOT / 'build/e01/Debug/e01.exe'), str(deps / 'ngspice/Spice64_dll/dll-vs/ngspice.dll'),
                str(deps / 'ngspice-console/Spice64/bin'), str(ROOT / 'tools/backend_probe/initialization'),
                'pending-stage', 'rc', str(output)]
        with pinned_netlist(stage, compiled['netlist']) as pinned:
            argv[4] = str(pinned)
            report['command'] = [v.replace(str(ROOT), '<repository>').replace(str(pinned), '<pinned-stage>') for v in argv]
            try:(stage / 'rc.cir').write_bytes(b'changed')
            except OSError as error:report['netlist_write_denied'] = {'winerror': error.winerror}
            else:raise AssertionError('Pinned netlist write permitted')
            try:stage.rename(stage.with_name('moved-stage'))
            except OSError as error:report['stage_rename_denied'] = {'winerror': error.winerror}
            else:raise AssertionError('Pinned stage rename permitted')
            with (output / 'engine.log').open('w', encoding='utf-8') as log:
                process = subprocess.run(argv, cwd=output, stdout=log, stderr=subprocess.STDOUT, timeout=30)
            assert (stage / 'rc.cir').read_bytes() == compiled['netlist']
        report['netlist_sha256'] = hashlib.sha256(compiled['netlist']).hexdigest()
        report['process_exit'] = process.returncode;assert process.returncode == 0
        metrics = json.loads((output / 'metrics.json').read_text(encoding='utf-8'));report['metrics'] = metrics
        assert metrics['idle_before_quit'] == 1 and metrics['quit_requested'] == 1 and metrics['exit_status'] == 0
        assert metrics['callback_fault'] == 0
        report['analytical'] = e01.analyze(output / 'first.csv', False, 0.005)
        report['project_time_tolerance_s'] = doc['temporal']['analog_time_tolerance_ps'] * 1e-12
        assert abs(report['analytical']['final_time_s'] - doc['temporal']['duration_ns'] * 1e-9) <= report['project_time_tolerance_s']
        report['project_voltage_tolerance_v'] = doc['temporal']['voltage_tolerance_uv'] * 1e-6
        assert report['analytical']['max_error_v'] <= report['project_voltage_tolerance_v']
        callbacks, vectors = e01.rows(output / 'callbacks.csv'), e01.rows(output / 'first.csv')
        assert len(callbacks) == len(vectors)
        assert all(all(a[k] == b[k] for k in ('time_s', 'input_v', 'output_v')) for a, b in zip(callbacks, vectors))
        report['callbacks_match_vectors'] = True;report['status'] = 'passed'
    except Exception as error:
        report['status'] = 'failed';report['error'] = str(error);raise
    finally:
        report['files_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in sorted(output.iterdir()) if p.is_file()}
        (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(json.dumps({k: report.get(k) for k in ('status', 'error', 'analytical', 'replay')}))


if __name__ == '__main__':
    main()
