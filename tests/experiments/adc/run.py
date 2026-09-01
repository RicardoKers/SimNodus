"""Run and analyze the complete predeclared E-04 ADC profile."""
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
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
REPETITIONS = 3
STATIC_INPUTS = [0, 825000, 1650000, 2475000, 3300000]
BOUNDARY_INPUTS = [805, 806, 3299999, 3400000]
ROUNDTRIP_INPUTS = [0, 825000, 1650000, 2475000, 3300000, 3400000]
CONVERSION_DURATIONS_US = [14, 20, 26, 41, 54, 68, 84, 252]

spec = importlib.util.spec_from_file_location('sn019', HERE.parent / 'renode-client/run.py')
sn019 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sn019)
check = sn019.check


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def expected_code(microvolts: int) -> int:
    return min(4095, microvolts * 4096 // 3300000)


def analyze(result: dict) -> dict:
    check(result['channel_count'] == 16, 'External ADC channel count changed')
    check(result['api_roundtrip_uv'] == ROUNDTRIP_INPUTS, 'IADC microvolt round-trip changed')
    invalid = result['invalid_channels']
    check(invalid['negative_code'] >= 0 and invalid['high_code'] >= 0,
          'Invalid ADC channels were not rejected')
    check(invalid['time_unchanged'] and invalid['valid_value_preserved'],
          'Invalid ADC channel attempt changed valid state or time')

    groups = {
        'static': (result['static'], STATIC_INPUTS),
        'boundaries': (result['boundaries'], BOUNDARY_INPUTS),
    }
    maximum_error = 0
    for name, (rows, expected_inputs) in groups.items():
        check([row['input_uv'] for row in rows] == expected_inputs, f'{name} input set changed')
        for row in rows:
            reference = expected_code(row['input_uv'])
            check(row['expected_code'] == reference, f'{name} host reference changed')
            error = abs(row['actual_code'] - reference)
            maximum_error = max(maximum_error, error)
            check(error <= 1, f'{name} code error exceeded one LSB')
            check(row['after_us'] - row['before_us'] == 60, f'{name} firmware interval changed')

    mapping = result['mapping']
    check([(row['channel'], row['input_uv']) for row in mapping] == [(0, 825000), (1, 2475000)],
          'ADC channel mapping case changed')
    for row in mapping:
        check(abs(row['actual_code'] - expected_code(row['input_uv'])) <= 1,
              'ADC channel mapping returned the wrong code')

    ramp = result['ramp']
    check(len(ramp) == 101, 'ADC ramp sample count changed')
    check([row['input_uv'] for row in ramp] == [index * 33000 for index in range(101)],
          'ADC ramp input sequence changed')
    codes = [row['actual_code'] for row in ramp]
    check(codes[0] == 0 and codes[-1] == 4095, 'ADC ramp endpoints changed')
    check(all(left <= right for left, right in zip(codes, codes[1:])), 'ADC ramp is not monotonic')
    for row in ramp:
        error = abs(row['actual_code'] - expected_code(row['input_uv']))
        maximum_error = max(maximum_error, error)
        check(error <= 1, 'ADC ramp code error exceeded one LSB')

    timing = result['timing']
    check([row['setting'] for row in timing] == list(range(8)), 'ADC timing settings changed')
    for row, duration in zip(timing, CONVERSION_DURATIONS_US):
        check(row['declared_duration_us'] == duration, 'ADC declared duration changed')
        check(row['completed_us'] - row['started_us'] == duration,
              'ADC EOC endpoint differs from the declared conversion duration')
        check(row['actual_code'] == expected_code(1650000), 'ADC timing case returned the wrong code')

    snapshot = result['start_snapshot']
    check(snapshot['updated_us'] - snapshot['started_us'] == 1, 'Snapshot update offset changed')
    check(snapshot['completed_us'] - snapshot['started_us'] == 14, 'Snapshot conversion duration changed')
    check(snapshot['actual_code'] == expected_code(snapshot['start_input_uv']),
          'In-flight conversion did not retain the SWSTART sample')
    check(snapshot['updated_input_uv'] == 2475000, 'Snapshot replacement input changed')
    disabled = result['disabled_start']
    check(disabled['checked_us'] - disabled['started_us'] == 20 and not disabled['eoc'],
          'Disabled ADC software start produced EOC')
    return {
        'maximum_code_error': maximum_error,
        'ramp_samples': len(ramp),
        'ramp_monotonic': True,
        'timing_durations_us': CONVERSION_DURATIONS_US,
        'sample_instant': 'accepted_SWSTART',
    }


def start_renode(args, directory: Path) -> tuple[subprocess.Popen, int, Path]:
    directory.mkdir(parents=True, exist_ok=True)
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    files = [
        args.prepared / 'LoopbackControl.cs', HERE / 'stm32f103_adc.cs',
        HERE / 'stm32f103c8-adc.repl', args.firmware / 'firmware.elf',
    ]
    check(not any(character.isspace() for path in files for character in str(path)),
          'Experiment paths must not contain whitespace')
    commands = [
        f'include @{files[0].as_posix()}',
        'mach create "sn015"',
        f'machine LoadPlatformDescription @{files[2].as_posix()}',
        f'sysbus LoadELF @{files[3].as_posix()}',
        'sysbus.cpu VectorTableOffset 0x08000000',
        'emulation SetGlobalQuantum "0.000001"',
        f'emulation CreateLoopbackControlServer "sn015-control" {port}',
    ]
    environment = os.environ.copy()
    environment['TEMP'] = environment['TMP'] = tempfile.mkdtemp(prefix='runtime-', dir=directory)
    log_path = directory / 'renode.log'
    log = log_path.open('w', encoding='utf-8')
    process = subprocess.Popen(
        [str(args.renode / 'Renode.exe'), '--disable-xwt', '--console', '--plain',
         '--config', str(directory / 'renode.config'), '--execute', '; '.join(commands)],
        stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT, text=True,
        env=environment, creationflags=subprocess.CREATE_NO_WINDOW,
    )
    process._simnodus_log = log
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        check(process.poll() is None, 'Renode exited during E-04 startup')
        endpoints = sn019.listeners(process.pid)
        if endpoints:
            check(endpoints == [{'address': '127.0.0.1', 'port': port, 'owner_matches': True}],
                  'Unexpected Renode listening endpoint')
            return process, port, log_path
        contents = log_path.read_text(encoding='utf-8', errors='replace')
        check(not any(text in contents for text in ('Errors during', 'Fatal error', 'error executing command')),
              'Renode E-04 startup failed; inspect its log')
        time.sleep(0.05)
    raise ValueError('Renode E-04 listener startup timed out')


def stop_renode(process: subprocess.Popen) -> int:
    if process.poll() is None:
        try:
            process.communicate('quit\n', timeout=10)
        except (subprocess.TimeoutExpired, OSError):
            process.kill()
            process.communicate(timeout=5)
    process._simnodus_log.close()
    check(process.returncode == 0, 'Renode E-04 shutdown was not normal')
    check(not sn019.listeners(process.pid), 'Renode E-04 listener survived shutdown')
    return process.returncode


def run_case(args, repetition: int) -> dict:
    directory = args.output / f'repetition-{repetition}'
    started_utc = datetime.now(timezone.utc).isoformat()
    started_monotonic = time.monotonic()
    process, port, log_path = start_renode(args, directory)
    native_exit = None
    try:
        with (directory / 'stdout.json').open('w', encoding='utf-8') as out, \
             (directory / 'stderr.log').open('w', encoding='utf-8') as err:
            native = subprocess.run(
                [str(args.native / 'e04_probe.exe'), str(port)], stdout=out, stderr=err,
                timeout=120, creationflags=subprocess.CREATE_NO_WINDOW,
            )
        native_exit = native.returncode
        check(native_exit == 0, f'E-04 native repetition {repetition} failed; inspect stderr.log')
        result = json.loads((directory / 'stdout.json').read_text(encoding='utf-8'))
        result['analysis'] = analyze(result)
    finally:
        renode_exit = stop_renode(process)
    supervision = {
        'started_utc': started_utc,
        'completed_utc': datetime.now(timezone.utc).isoformat(),
        'duration_s': time.monotonic() - started_monotonic,
        'native_process_exit': native_exit,
        'renode_process_exit': renode_exit,
    }
    (directory / 'supervision.json').write_text(json.dumps(supervision, indent=2) + '\n', encoding='utf-8')
    log = log_path.read_text(encoding='utf-8', errors='replace')
    check(not any(text in log for text in ('Errors during', 'Unhandled exception', 'CPU was halted', 'unmapped')),
          f'Renode reported an E-04 execution error in repetition {repetition}')
    invalid_diagnostic = 'ADC command error: ADC channel must be in [0, 15]'
    disabled_diagnostic = 'Ignoring software start while ADC is disabled'
    check(log.count(invalid_diagnostic) == 2 and log.count(disabled_diagnostic) == 1,
          'Expected E-04 adverse-case diagnostics changed')
    unexpected_diagnostics = [
        line for line in log.splitlines()
        if ('[ERROR]' in line or '[WARNING]' in line)
        and invalid_diagnostic not in line and disabled_diagnostic not in line
    ]
    check(not unexpected_diagnostics, 'Renode emitted an unexpected E-04 warning or error')
    result['supervision'] = supervision
    result['loopback_verified'] = result['listener_removed'] = True
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renode', type=Path, default=ROOT / 'build/deps/renode/renode_1.16.1-dotnet_portable')
    parser.add_argument('--prepared', type=Path, default=ROOT / 'build/sn019/generated')
    parser.add_argument('--firmware', type=Path, default=ROOT / 'build/sn015/firmware')
    parser.add_argument('--native', type=Path, default=ROOT / 'build/sn015/native/Debug')
    parser.add_argument('--audit', type=Path, default=ROOT / 'build/sn015/audit')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/sn015/results')
    args = parser.parse_args()
    for name in ('renode', 'prepared', 'firmware', 'native', 'audit', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    args.output.mkdir(parents=True, exist_ok=True)

    subprocess.run([sys.executable, str(ROOT / 'tools/check_backend_assets.py'), '--renode-only',
                    '--root', str(args.renode.parents[1])], check=True)
    subprocess.run([sys.executable, str(HERE / 'audit.py'), '--root', str(args.audit)], check=True)
    audit = json.loads((args.audit / 'audit.json').read_text(encoding='utf-8'))
    build = json.loads((args.firmware / 'build.json').read_text(encoding='utf-8'))
    check(sha(args.firmware / 'firmware.elf') == build['elf_sha256'], 'E-04 ELF changed after build')
    for name, expected in build['sources_sha256'].items():
        check(sha(HERE / name) == expected, 'E-04 firmware source changed after build')
    control = json.loads((args.prepared / 'provenance.json').read_text(encoding='utf-8'))
    for name, expected in control.items():
        if name.startswith('generated/'):
            check(sha(args.prepared / Path(name).name) == expected, 'Prepared control artifact changed')

    report = {
        'status': 'passed',
        'profile': {
            'reference_uv': 3300000,
            'resolution_bits': 12,
            'quantization': 'min(4095,floor(input_uv*4096/3300000))',
            'adc_clock_hz': 1000000,
            'repetitions': REPETITIONS,
        },
        'firmware': build,
        'audit': audit,
        'control_provenance': control,
        'sha256': {
            path.name: sha(path) for path in [
                HERE / 'stm32f103_adc.cs', HERE / 'stm32f103c8-adc.repl',
                HERE / 'probe.cpp', HERE / 'run.py', HERE / 'CMakeLists.txt',
                args.native / 'e04_probe.exe', args.prepared / 'LoopbackControl.cs',
            ]
        },
        'runs': [],
    }
    try:
        for repetition in range(1, REPETITIONS + 1):
            result = run_case(args, repetition)
            report['runs'].append(result)
            print(f'repetition {repetition}: passed', flush=True)
        signatures = []
        for result in report['runs']:
            signature = dict(result)
            signature.pop('supervision')
            signatures.append(signature)
        check(all(signature == signatures[0] for signature in signatures[1:]),
              'Repeated E-04 discrete results changed')
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        report['status'] = 'failed'
        report['error'] = str(error)
    (args.output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
