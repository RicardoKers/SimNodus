"""Run the owned STM32 ELF through the verified loopback/native control profile."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
spec = importlib.util.spec_from_file_location('sn019', HERE.parent / 'renode-client/run.py')
sn019 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sn019)
check = sn019.check


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args, step: int) -> dict:
    directory = args.output / str(step); directory.mkdir(parents=True, exist_ok=True)
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0)); port = reservation.getsockname()[1]
    files = [args.prepared / 'LoopbackControl.cs', HERE / 'stm32f103c8.repl', args.firmware / 'firmware.elf']
    check(not any(c.isspace() for p in files for c in str(p)), 'Experiment paths must not contain whitespace')
    commands = [f'include @{files[0].as_posix()}', 'mach create "sn012"',
                f'machine LoadPlatformDescription @{files[1].as_posix()}',
                'sysbus WriteDoubleWord 0x40021000 0x83',
                f'sysbus LoadELF @{files[2].as_posix()}', 'sysbus.cpu VectorTableOffset 0x08000000',
                f'emulation SetGlobalQuantum "{step / 1000000:.6f}"',
                f'emulation CreateLoopbackControlServer "sn012-control" {port}']
    environment = os.environ.copy()
    environment['TEMP'] = environment['TMP'] = tempfile.mkdtemp(prefix='runtime-', dir=directory)
    log_path = directory / 'renode.log'
    result = {}
    with log_path.open('w', encoding='utf-8') as log:
        process = subprocess.Popen([str(args.renode / 'Renode.exe'), '--disable-xwt', '--console', '--plain',
            '--config', str(directory / 'renode.config'), '--execute', '; '.join(commands)],
            stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT, text=True,
            env=environment, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                check(process.poll() is None, 'Renode exited during startup')
                endpoints = sn019.listeners(process.pid)
                if endpoints:
                    check(endpoints == [{'address': '127.0.0.1', 'port': port, 'owner_matches': True}],
                          'Unexpected listening endpoint')
                    break
                contents = log_path.read_text(encoding='utf-8', errors='replace')
                check(not any(s in contents for s in ('Errors during', 'Fatal error', 'error executing command')),
                      'Renode platform/startup failed; inspect log')
                time.sleep(0.05)
            else:
                raise ValueError('Renode listener startup timed out')
            with (directory / 'stdout.json').open('w', encoding='utf-8') as out, (directory / 'stderr.log').open('w', encoding='utf-8') as err:
                native = subprocess.run([str(args.native / 'stm32_probe.exe'), str(port), str(step)],
                    stdout=out, stderr=err, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
            check(native.returncode == 0, f'Native firmware probe failed ({native.returncode}); inspect {step}/stderr.log')
            result = json.loads((directory / 'stdout.json').read_text(encoding='utf-8'))
        finally:
            if process.poll() is None:
                try:
                    process.communicate('quit\n', timeout=10)
                except (subprocess.TimeoutExpired, OSError):
                    process.kill(); process.communicate(timeout=5)
                    result['forced_cleanup'] = True
            result['process_exit'] = process.returncode
    check(result['process_exit'] == 0 and not result.get('forced_cleanup'), 'Renode shutdown was not normal')
    check(not sn019.listeners(process.pid), 'Renode listener survived shutdown')
    result['loopback_verified'] = result['listener_removed'] = True
    log = log_path.read_text(encoding='utf-8', errors='replace')
    check(not any(s in log for s in ('Errors during', 'Unhandled exception', 'CPU was halted', 'unmapped')),
          'Renode reported a platform/execution error; inspect log')
    events = result['events']
    check(events and all(e['during_run'] for e in events), 'Callbacks were not delivered inside RunFor')
    check(all(e['request_begin_us'] <= e['timestamp_us'] <= e['request_end_us'] for e in events),
          'Event timestamp escaped its requested interval')
    check(all(a['timestamp_us'] <= b['timestamp_us'] for a, b in zip(events, events[1:])), 'Event timestamps reversed')
    ticks = [e for e in events if e['pin'] == 0]
    check(len(ticks) >= 6 and all(a['state'] != b['state'] for a, b in zip(ticks, ticks[1:])),
          'Missing/duplicated periodic output transitions')
    gaps = [b['timestamp_us'] - a['timestamp_us'] for a, b in zip(ticks, ticks[1:])]
    result['tick_gaps_us'] = gaps
    check(len(ticks) == 23 and result['final_time_us'] == 23900, 'Periodic/final profile changed')
    check([s['exti_count'] for s in result['snapshots']] == [0, 0, 1, 1, 2, 2, 4, 5],
          'Input/interrupt observations changed')
    expected_modes = {
        'floating': (0, 0, 1), 'pull_down': (1, 0, 1), 'pull_up': (1, 0, 1),
        'analog': (1, 0, 1), 'push_pull': (1, 1, 1), 'open_drain': (1, 1, 1),
        'alternate_push_pull': (0, 0, 0)}
    check({m['name']: (m['before'], m['injected_low'], m['injected_high']) for m in result['modes']} == expected_modes,
          'Recorded GPIO mode characterization changed')
    check(result['rcc_audit'] == {'stored_enable': 0, 'ticks_before': 21, 'ticks_after': 23},
          'Recorded RCC-stub behavior changed')
    if step == 100:
        check(all(abs(gap - 1000) <= 100 for gap in gaps), 'Fine-quantum spacing exceeded the predeclared tolerance')
    result['status'] = 'passed'
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renode', type=Path, default=ROOT / 'build/deps/renode/renode_1.16.1-dotnet_portable')
    parser.add_argument('--prepared', type=Path, default=ROOT / 'build/sn019/generated')
    parser.add_argument('--firmware', type=Path, default=ROOT / 'build/sn012/firmware')
    parser.add_argument('--native', type=Path, default=ROOT / 'build/sn012/native/Debug')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/sn012/results')
    args = parser.parse_args()
    for name in ('renode', 'prepared', 'firmware', 'native', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/check_backend_assets.py'), '--renode-only',
                    '--root', str(args.renode.parents[1])], check=True)
    build = json.loads((args.firmware / 'build.json').read_text(encoding='utf-8'))
    check(sha(args.firmware / 'firmware.elf') == build['elf_sha256'], 'ELF changed after build')
    for name, expected in build['sources_sha256'].items():
        check(sha(HERE / name) == expected, 'Firmware sources changed after build')
    control = json.loads((args.prepared / 'provenance.json').read_text(encoding='utf-8'))
    for name, expected in control.items():
        if name.startswith('generated/'):
            check(sha(args.prepared / Path(name).name) == expected, 'Prepared control artifact changed')
    report = {'status': 'passed', 'firmware': build, 'profiles': {}, 'sha256': {
        p.name: sha(p) for p in [*HERE.glob('*.cpp'), *HERE.glob('*.py'), HERE / 'stm32f103c8.repl',
                                HERE / 'CMakeLists.txt', HERE / 'audit-sources.json',
                                args.native / 'stm32_probe.exe', args.prepared / 'LoopbackControl.cs']},
              'control_provenance': control}
    for step in (100, 1000):
        try:
            result = run(args, step)
        except (ValueError, OSError, subprocess.TimeoutExpired) as error:
            result = {'status': 'failed', 'error': str(error)}; report['status'] = 'failed'
        report['profiles'][str(step)] = result
        print(step, result['status'], result.get('error', ''), flush=True)
    (args.output / 'summary.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
