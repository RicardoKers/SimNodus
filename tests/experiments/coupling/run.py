"""Run and analyze the predeclared E-03 ngspice/Renode coupling profiles."""
from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
import math
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
VOLTAGE_TOLERANCE_V = 0.0165
CROSSING_TOLERANCE_S = 2e-6
TAU_S = 1e-3
FINAL_TIME_US = 10000
QUANTA_US = (1000, 100, 20)
REPETITIONS = 3

spec = importlib.util.spec_from_file_location('sn019', HERE.parent / 'renode-client/run.py')
sn019 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sn019)
check = sn019.check


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding='utf-8', newline='') as stream:
        return list(csv.DictReader(stream))


def numeric_rows(path: Path) -> list[dict[str, float]]:
    return [{key: float(value) for key, value in row.items()} for row in rows(path)]


def response(schedule: list[dict[str, float]], sample_time: float) -> float:
    result = 0.0
    previous = 0.0
    for edge in schedule[1:]:
        edge_time = edge['time_s']
        if edge_time > sample_time:
            break
        value = 3.3 if edge['state'] else 0.0
        result += (value - previous) * -math.expm1(-(sample_time - edge_time) / TAU_S)
        previous = value
    return result


def expected_crossings(schedule: list[dict[str, float]]) -> list[tuple[bool, float]]:
    result = []
    voltage = 0.0
    logic = False
    events = schedule[1:] + [{'time_s': FINAL_TIME_US * 1e-6, 'state': schedule[-1]['state']}]
    source = 0.0
    previous_time = 0.0
    for event in events:
        end = event['time_s']
        threshold = 0.99 if logic else 2.31
        if source != threshold and ((voltage - threshold) * (source - threshold) <= 0):
            ratio = (threshold - source) / (voltage - source)
            if ratio > 0:
                crossing = previous_time - TAU_S * math.log(ratio)
                if previous_time <= crossing <= end:
                    logic = not logic
                    result.append((logic, crossing))
        voltage = source + (voltage - source) * math.exp(-(end - previous_time) / TAU_S)
        source = 3.3 if event['state'] else 0.0
        previous_time = end
    return result


def analyze_analog(directory: Path, quantum_us: int | None) -> dict:
    samples = numeric_rows(directory / 'analog.csv')
    schedule = numeric_rows(directory / 'schedule.csv')
    thresholds = numeric_rows(directory / 'thresholds.csv')
    boundaries = numeric_rows(directory / 'boundaries.csv')
    check(len(samples) > 1000, 'Insufficient accepted ngspice samples')
    check(all(math.isfinite(value) for row in samples for value in row.values()), 'Nonfinite analog data')
    check(all(right['time_s'] > left['time_s'] for left, right in zip(samples, samples[1:])),
          'Accepted ngspice timestamps were not strictly increasing')
    check(len(schedule) == 3 and [int(row['state']) for row in schedule] == [0, 1, 0],
          'Firmware-derived source schedule changed')
    errors = [(abs(row['output_v'] - response(schedule, row['time_s'])), row['time_s']) for row in samples]
    gated = [error for error, stamp in errors
             if all(abs(stamp - edge['time_s']) > 2e-6 for edge in schedule[1:])]
    max_error = max(gated)
    check(max_error <= VOLTAGE_TOLERANCE_V, 'RC voltage error exceeded 16.5 mV')
    expected = expected_crossings(schedule)
    check(len(expected) == 2 and len(thresholds) == 2, 'Expected rising/falling crossing pair was not preserved')
    crossing_errors = []
    delays = []
    for (expected_state, expected_time), actual in zip(expected, thresholds):
        check(bool(int(actual['state'])) == expected_state, 'Threshold state ordering changed')
        error = abs(actual['crossing_s'] - expected_time)
        crossing_errors.append(error)
        check(error <= CROSSING_TOLERANCE_S, 'Threshold localization exceeded 2 us')
        if quantum_us is not None:
            check(int(actual['applied']) == 1, 'Detected threshold was not applied to Renode')
            delay = actual['apply_us'] - actual['ceil_us']
            delays.append(delay)
            check(0 <= delay <= quantum_us + 2, 'Sampled feedback delay exceeded Q + 2 us')
    qualifying_separation_us = (thresholds[1]['crossing_s'] - thresholds[0]['crossing_s']) * 1e6
    if quantum_us is not None and qualifying_separation_us > quantum_us + 2:
        check(len(thresholds) == 2 and all(int(row['applied']) for row in thresholds),
              'Qualifying threshold pulse was missed')
    check(boundaries and int(boundaries[-1]['requested_us']) == FINAL_TIME_US,
          'Final exchange boundary changed')
    return {
        'samples': len(samples),
        'source_edges': len(schedule) - 1,
        'source_edges_discovered_late': sum(int(row['discovered_late']) for row in schedule),
        'max_voltage_error_v': max_error,
        'max_crossing_error_s': max(crossing_errors),
        'crossings': [{'state': bool(state), 'time_s': stamp} for state, stamp in expected],
        'measured_crossings_s': [row['crossing_s'] for row in thresholds],
        'delays_us': delays,
        'late_events': sum(int(row['late']) for row in thresholds),
        'qualifying_threshold_separation_us': qualifying_separation_us,
        'exact_joint_boundaries': sum(int(row['exact_joint_boundary']) for row in boundaries),
        'boundary_count': len(boundaries),
    }


def start_renode(args, directory: Path, quantum_us: int) -> tuple[subprocess.Popen, int, Path]:
    directory.mkdir(parents=True, exist_ok=True)
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    platform = HERE.parent / 'renode-stm32/stm32f103c8.repl'
    files = [args.prepared / 'LoopbackControl.cs', platform, args.firmware / 'firmware.elf']
    check(not any(character.isspace() for path in files for character in str(path)),
          'Experiment paths must not contain whitespace')
    quantum = quantum_us if quantum_us else 100
    commands = [f'include @{files[0].as_posix()}', 'mach create "sn014"',
                f'machine LoadPlatformDescription @{files[1].as_posix()}',
                'sysbus WriteDoubleWord 0x40021000 0x83',
                f'sysbus LoadELF @{files[2].as_posix()}', 'sysbus.cpu VectorTableOffset 0x08000000',
                f'emulation SetGlobalQuantum "{quantum / 1000000:.6f}"',
                f'emulation CreateLoopbackControlServer "sn014-control" {port}']
    environment = os.environ.copy()
    environment['TEMP'] = environment['TMP'] = tempfile.mkdtemp(prefix='runtime-', dir=directory)
    log_path = directory / 'renode.log'
    log = log_path.open('w', encoding='utf-8')
    process = subprocess.Popen([str(args.renode / 'Renode.exe'), '--disable-xwt', '--console', '--plain',
        '--config', str(directory / 'renode.config'), '--execute', '; '.join(commands)],
        stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT, text=True,
        env=environment, creationflags=subprocess.CREATE_NO_WINDOW)
    process._simnodus_log = log
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        check(process.poll() is None, 'Renode exited during E-03 startup')
        endpoints = sn019.listeners(process.pid)
        if endpoints:
            check(endpoints == [{'address': '127.0.0.1', 'port': port, 'owner_matches': True}],
                  'Unexpected Renode listening endpoint')
            return process, port, log_path
        contents = log_path.read_text(encoding='utf-8', errors='replace')
        check(not any(text in contents for text in ('Errors during', 'Fatal error', 'error executing command')),
              'Renode platform/startup failed; inspect its log')
        time.sleep(0.05)
    raise ValueError('Renode listener startup timed out')


def stop_renode(process: subprocess.Popen, expect_normal: bool) -> int:
    if process.poll() is None:
        try:
            process.communicate('quit\n', timeout=10)
        except (subprocess.TimeoutExpired, OSError):
            process.kill()
            process.communicate(timeout=5)
    process._simnodus_log.close()
    if expect_normal:
        check(process.returncode == 0, 'Renode shutdown was not normal')
        check(not sn019.listeners(process.pid), 'Renode listener survived shutdown')
    return process.returncode


def native_command(args, port: int, mode: str, quantum_us: int,
                   shift_ns: int, directory: Path) -> list[str]:
    return [str(args.native / 'e03_probe.exe'), str(port), mode, str(quantum_us), str(shift_ns),
            str(args.deps / 'ngspice/Spice64_dll/dll-vs/ngspice.dll'),
            str(args.deps / 'ngspice-console/Spice64/bin'),
            str(ROOT / 'tools/backend_probe/initialization'), str(HERE / 'rc.cir'), str(directory)]


def run_case(args, name: str, mode: str, quantum_us: int = 0, shift_ns: int = 0) -> dict:
    directory = args.output / name
    started_utc = datetime.now(timezone.utc).isoformat()
    started_monotonic = time.monotonic()
    process, port, log_path = start_renode(args, directory, quantum_us)
    native_exit = None
    try:
        with (directory / 'engine.log').open('w', encoding='utf-8') as log:
            native = subprocess.run(native_command(args, port, mode, quantum_us, shift_ns, directory),
                                    stdout=log, stderr=subprocess.STDOUT, timeout=90,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
        check(native.returncode == 0, f'E-03 native case failed ({name}); inspect engine.log')
        native_exit = native.returncode
        result = json.loads((directory / 'result.json').read_text(encoding='utf-8'))
    finally:
        renode_exit = stop_renode(process, expect_normal=True)
    supervision = {
        'started_utc': started_utc,
        'completed_utc': datetime.now(timezone.utc).isoformat(),
        'duration_s': time.monotonic() - started_monotonic,
        'native_process_exit': native_exit,
        'renode_process_exit': renode_exit,
    }
    (directory / 'supervision.json').write_text(
        json.dumps(supervision, indent=2) + '\n', encoding='utf-8')
    log = log_path.read_text(encoding='utf-8', errors='replace')
    check(not any(text in log for text in ('Errors during', 'Unhandled exception', 'CPU was halted', 'unmapped')),
          f'Renode reported an execution error in {name}')
    result['loopback_verified'] = result['listener_removed'] = True
    result['supervision'] = supervision
    if mode in ('replay', 'sampled'):
        result['analysis'] = analyze_analog(directory, quantum_us if mode == 'sampled' else None)
    if mode == 'digital':
        pulse_rows = numeric_rows(directory / 'digital.csv')
        check(pulse_rows and result['late_input_rejected'], 'Digital adversarial evidence is incomplete')
        check(result['same_time_interrupt_delta'] <= 1, 'Same-time opposite levels preserved two interrupts')
        result['pulses'] = pulse_rows
    return result


def run_failure(args, repetition: int) -> dict:
    directory = args.output / f'failure-r{repetition}'
    started_utc = datetime.now(timezone.utc).isoformat()
    started_monotonic = time.monotonic()
    process, port, _ = start_renode(args, directory, 100)
    engine_log = (directory / 'engine.log').open('w', encoding='utf-8')
    native = subprocess.Popen(native_command(args, port, 'failure', 100, 0, directory),
                              stdout=engine_log, stderr=subprocess.STDOUT,
                              creationflags=subprocess.CREATE_NO_WINDOW)
    marker = directory / 'accepted-request.marker'
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline and not marker.exists() and native.poll() is None:
        time.sleep(0.001)
    check(marker.exists(), 'Failure case did not submit its long RunFor request')
    marker_observed_s = time.monotonic() - started_monotonic
    process.kill()
    process.wait(timeout=5)
    process._simnodus_log.close()
    native.wait(timeout=10)
    engine_log.close()
    check(native.returncode == 0, 'Native client did not report the injected backend failure')
    result = json.loads((directory / 'result.json').read_text(encoding='utf-8'))
    check(result['status'] == 'expected_failure_observed' and not result['joint_commit'],
          'Backend failure was incorrectly committed')
    supervision = {
        'started_utc': started_utc,
        'completed_utc': datetime.now(timezone.utc).isoformat(),
        'duration_s': time.monotonic() - started_monotonic,
        'request_marker_observed_s': marker_observed_s,
        'native_process_exit': native.returncode,
        'terminated_renode_process_exit': process.returncode,
    }
    (directory / 'supervision.json').write_text(
        json.dumps(supervision, indent=2) + '\n', encoding='utf-8')
    recovery = run_case(args, f'recovery-r{repetition}', 'recovery', 100, 0)
    check(recovery['fresh_backend'] and recovery['joint_commit_us'] == 600,
          'Fresh worker did not recover after injected termination')
    return {'failure': result, 'failure_supervision': supervision,
            'recovery': recovery, 'terminated_process_exit': process.returncode}


def discrete_signature(directory: Path) -> dict:
    schedule = rows(directory / 'schedule.csv')
    thresholds = rows(directory / 'thresholds.csv')
    gpio = rows(directory / 'gpio.csv')
    return {
        'schedule': [(row['origin_gpio_us'], row['state']) for row in schedule],
        'thresholds': [(row['state'], row['ceil_us'], row['apply_us'], row['late']) for row in thresholds],
        'gpio': [(row['pin'], row['state'], row['timestamp_us']) for row in gpio],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renode', type=Path, default=ROOT / 'build/deps/renode/renode_1.16.1-dotnet_portable')
    parser.add_argument('--prepared', type=Path, default=ROOT / 'build/sn019/generated')
    parser.add_argument('--firmware', type=Path, default=ROOT / 'build/sn014/firmware')
    parser.add_argument('--native', type=Path, default=ROOT / 'build/sn014/native/Debug')
    parser.add_argument('--deps', type=Path, default=ROOT / 'build/deps')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/sn014/results')
    parser.add_argument('--quick', action='store_true', help='One replay and one standard 100 us sampled run for development only')
    parser.add_argument('--failure-only', action='store_true', help='One injected failure/recovery pair for development only')
    args = parser.parse_args()
    for name in ('renode', 'prepared', 'firmware', 'native', 'deps', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/check_backend_assets.py'),
                    '--root', str(args.deps)], check=True)
    build = json.loads((args.firmware / 'build.json').read_text(encoding='utf-8'))
    check(sha(args.firmware / 'firmware.elf') == build['elf_sha256'], 'E-03 ELF changed after build')
    for name, expected in build['sources_sha256'].items():
        check(sha(HERE / name) == expected, 'E-03 firmware source changed after build')
    if args.failure_only:
        result = run_failure(args, 1)
        (args.output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print('failure-recovery-r1 passed')
        return 0
    report = {'status': 'passed', 'contract': {
        'voltage_tolerance_v': VOLTAGE_TOLERANCE_V,
        'crossing_tolerance_s': CROSSING_TOLERANCE_S,
        'quanta_us': list(QUANTA_US), 'repetitions': REPETITIONS,
        'thresholds_v': {'rising': 2.31, 'falling': 0.99}},
        'firmware': build, 'replay': [], 'sampled': {}, 'boundary': {},
        'digital': {}, 'failure_recovery': []}
    repetitions = 1 if args.quick else REPETITIONS
    quanta = (100,) if args.quick else QUANTA_US
    replay_signatures = []
    for repetition in range(1, repetitions + 1):
        name = f'replay-r{repetition}'
        result = run_case(args, name, 'replay')
        result['name'] = name
        report['replay'].append(result)
        replay_signatures.append(discrete_signature(args.output / name))
        print(name, 'passed', flush=True)
    check(all(signature == replay_signatures[0] for signature in replay_signatures[1:]),
          'Replay discrete ordering changed between repetitions')
    for quantum in quanta:
        key = str(quantum)
        report['sampled'][key] = []
        signatures = []
        for repetition in range(1, repetitions + 1):
            name = f'sampled-q{quantum}-r{repetition}'
            result = run_case(args, name, 'sampled', quantum, 0)
            result['name'] = name
            report['sampled'][key].append(result)
            signatures.append(discrete_signature(args.output / name))
            print(name, 'passed', flush=True)
        check(all(signature == signatures[0] for signature in signatures[1:]),
              f'Sampled Q={quantum} discrete ordering changed between repetitions')
        if args.quick:
            continue
        first_schedule = numeric_rows(args.output / f'sampled-q{quantum}-r1/schedule.csv')
        origin_rise_us = int(first_schedule[1]['origin_gpio_us'])
        boundary_us = 4000
        report['boundary'][key] = {}
        for relation in (-1, 0, 1):
            desired_crossing_us = boundary_us + relation
            desired_rise_us = desired_crossing_us - (-TAU_S * math.log(0.30) * 1e6)
            shift_ns = round((desired_rise_us - origin_rise_us) * 1000)
            relation_key = { -1: 'before', 0: 'exact', 1: 'after' }[relation]
            values = []
            signatures = []
            for repetition in range(1, REPETITIONS + 1):
                name = f'boundary-q{quantum}-{relation_key}-r{repetition}'
                result = run_case(args, name, 'sampled', quantum, shift_ns)
                measured = result['analysis']['measured_crossings_s'][0] * 1e6
                check(abs(measured - desired_crossing_us) <= 2,
                      'Boundary crossing did not meet its predeclared T-1/T/T+1 position')
                result['name'] = name
                result['desired_crossing_us'] = desired_crossing_us
                values.append(result)
                signatures.append(discrete_signature(args.output / name))
                print(name, 'passed', flush=True)
            check(all(signature == signatures[0] for signature in signatures[1:]),
                  f'Boundary Q={quantum} {relation_key} ordering changed between repetitions')
            report['boundary'][key][relation_key] = values
        report['digital'][key] = []
        digital_signatures = []
        for repetition in range(1, REPETITIONS + 1):
            name = f'digital-q{quantum}-r{repetition}'
            result = run_case(args, name, 'digital', quantum, 0)
            result['name'] = name
            report['digital'][key].append(result)
            digital_signatures.append((
                [(pulse['width_us'], pulse['interrupt_delta']) for pulse in result['pulses']],
                result['same_time_interrupt_delta'], result['late_input_rejected']))
            print(name, 'passed', flush=True)
        check(all(signature == digital_signatures[0] for signature in digital_signatures[1:]),
              f'Digital Q={quantum} observations changed between repetitions')
    if not args.quick:
        for repetition in range(1, REPETITIONS + 1):
            result = run_failure(args, repetition)
            report['failure_recovery'].append(result)
            print(f'failure-recovery-r{repetition}', 'passed', flush=True)
        check(all(result['failure'] == report['failure_recovery'][0]['failure']
                  and result['recovery']['joint_commit_us'] == 600
                  for result in report['failure_recovery']),
              'Failure/recovery observations changed between repetitions')
    inputs = [*HERE.glob('*.cpp'), *HERE.glob('*.c'), *HERE.glob('*.h'), *HERE.glob('*.py'),
              HERE / 'firmware.ld', HERE / 'rc.cir', HERE / 'CMakeLists.txt',
              args.native / 'e03_probe.exe', args.prepared / 'LoopbackControl.cs']
    report['sha256'] = {path.name: sha(path) for path in inputs}
    (args.output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('E-03', report['status'])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
