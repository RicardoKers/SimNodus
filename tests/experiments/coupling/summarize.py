"""Create a small, reviewable E-03 evidence summary from a complete raw run."""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def maximum(values):
    return max(value for group in values for value in group)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    source = json.loads(args.input.read_text(encoding='utf-8'))
    replay = source['replay']
    sampled = source['sampled']
    boundary = source['boundary']
    digital = source['digital']
    failures = source['failure_recovery']
    if any(item['analysis'] != replay[0]['analysis'] for item in replay[1:]):
        raise ValueError('Replay analysis changed between repetitions')
    for quantum, runs in sampled.items():
        signature = (runs[0]['analysis']['delays_us'], runs[0]['analysis']['late_events'])
        if any((item['analysis']['delays_us'], item['analysis']['late_events']) != signature
               for item in runs[1:]):
            raise ValueError(f'Sampled Q={quantum} analysis changed between repetitions')
    for quantum, relations in boundary.items():
        for relation, runs in relations.items():
            signature = (runs[0]['analysis']['measured_crossings_s'], runs[0]['analysis']['delays_us'])
            if any((item['analysis']['measured_crossings_s'], item['analysis']['delays_us']) != signature
                   for item in runs[1:]):
                raise ValueError(f'Boundary Q={quantum} {relation} changed between repetitions')
    for quantum, runs in digital.items():
        signature = (runs[0]['pulses'], runs[0]['same_time_interrupt_delta'], runs[0]['late_input_rejected'])
        if any((item['pulses'], item['same_time_interrupt_delta'], item['late_input_rejected']) != signature
               for item in runs[1:]):
            raise ValueError(f'Digital Q={quantum} observations changed between repetitions')
    if any(item['failure'] != failures[0]['failure']
           or item['recovery']['joint_commit_us'] != 600 for item in failures[1:]):
        raise ValueError('Failure/recovery observations changed between repetitions')
    supervised = [*replay]
    for runs in sampled.values():
        supervised.extend(runs)
    for relations in boundary.values():
        for runs in relations.values():
            supervised.extend(runs)
    for runs in digital.values():
        supervised.extend(runs)
    durations = [item['supervision']['duration_s'] for item in supervised]
    durations.extend(item['failure_supervision']['duration_s'] for item in failures)
    durations.extend(item['recovery']['supervision']['duration_s'] for item in failures)
    if len(durations) != 54:
        raise ValueError(f'Expected 54 supervised backend processes, found {len(durations)}')
    compact = {
        'schema_version': 1,
        'experiment': 'E-03',
        'task': 'SN-014',
        'date': '2026-08-31',
        'result': 'passed_known_schedule_replay_and_sampled_approximation',
        'live_causal_feedback': 'unsupported',
        'complete_local_run': {
            'isolated_case_executions': 54,
            'fresh_renode_processes': 54,
            'native_host_processes': 54,
            'ngspice_dll_case_executions': 39,
            'repetitions_per_case': 3,
            'discrete_order_repeated': True,
            'final_executable_sha256': source['sha256']['e03_probe.exe'],
            'firmware_elf_sha256': source['firmware']['elf_sha256'],
            'identical_firmware_rebuild': source['firmware']['identical_rebuild'],
            'wall_supervision_recorded': True,
            'minimum_case_duration_s': min(durations),
            'maximum_case_duration_s': max(durations),
            'total_case_duration_s': sum(durations),
        },
        'contract': source['contract'],
        'known_schedule_replay': {
            'runs': len(replay),
            'exact_final_joint_boundaries': sum(
                item['analysis']['exact_joint_boundaries'] for item in replay),
            'maximum_voltage_error_v': max(
                item['analysis']['max_voltage_error_v'] for item in replay),
            'maximum_crossing_error_s': max(
                item['analysis']['max_crossing_error_s'] for item in replay),
            'accepted_crossings_per_run': 2,
        },
        'sampled_profiles': {},
        'boundary_cases': {},
        'digital_pulses': {},
        'failure_recovery': {
            'runs': len(failures),
            'joint_commits_after_failure': sum(
                int(item['failure']['joint_commit']) for item in failures),
            'fresh_recoveries_at_us': [
                item['recovery']['joint_commit_us'] for item in failures],
            'connection_error_codes': [
                item['failure']['error_code'] for item in failures],
        },
    }
    for quantum, runs in sampled.items():
        delays = [item['analysis']['delays_us'] for item in runs]
        compact['sampled_profiles'][quantum] = {
            'runs': len(runs),
            'rising_delays_us': [item[0] for item in delays],
            'falling_delays_us': [item[1] for item in delays],
            'maximum_delay_us': maximum(delays),
            'delay_limit_us': int(quantum) + 2,
            'late_thresholds_per_run': [item['analysis']['late_events'] for item in runs],
            'late_source_edges_per_run': [
                item['analysis']['source_edges_discovered_late'] for item in runs],
            'exact_joint_boundaries_per_run': [
                item['analysis']['exact_joint_boundaries'] for item in runs],
            'boundary_count_per_run': [item['analysis']['boundary_count'] for item in runs],
            'maximum_voltage_error_v': max(
                item['analysis']['max_voltage_error_v'] for item in runs),
            'maximum_crossing_error_s': max(
                item['analysis']['max_crossing_error_s'] for item in runs),
        }
    for quantum, relations in boundary.items():
        compact['boundary_cases'][quantum] = {}
        for relation, runs in relations.items():
            compact['boundary_cases'][quantum][relation] = {
                'desired_crossing_us': runs[0]['desired_crossing_us'],
                'measured_crossing_us': [
                    item['analysis']['measured_crossings_s'][0] * 1e6 for item in runs],
                'application_delays_us': [
                    item['analysis']['delays_us'][0] for item in runs],
            }
    for quantum, runs in digital.items():
        compact['digital_pulses'][quantum] = {
            'runs': len(runs),
            'width_to_interrupt_delta': {
                str(int(pulse['width_us'])): int(pulse['interrupt_delta'])
                for pulse in runs[0]['pulses']},
            'same_time_opposite_level_interrupts': [
                item['same_time_interrupt_delta'] for item in runs],
            'past_inputs_rejected': [item['late_input_rejected'] for item in runs],
        }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(compact, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
