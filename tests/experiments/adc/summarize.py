"""Create small, reviewable E-04 evidence from a complete raw run."""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def expected_code(microvolts: int) -> int:
    return min(4095, microvolts * 4096 // 3300000)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    source = json.loads(args.input.read_text(encoding='utf-8'))
    if source['status'] != 'passed' or len(source['runs']) != 3:
        raise ValueError('E-04 compact evidence requires three passing runs')
    signatures = []
    for run in source['runs']:
        signature = dict(run)
        signature.pop('supervision')
        signatures.append(signature)
    if any(signature != signatures[0] for signature in signatures[1:]):
        raise ValueError('E-04 repeated discrete evidence changed')

    run = source['runs'][0]
    rows = [*run['static'], *run['boundaries'], *run['mapping'], *run['ramp']]
    maximum_error = max(abs(row['actual_code'] - expected_code(row['input_uv'])) for row in rows)
    if maximum_error > 1:
        raise ValueError('E-04 compact evidence exceeds the one-code gate')
    timing = run['timing']
    if any(row['completed_us'] - row['started_us'] != row['declared_duration_us'] for row in timing):
        raise ValueError('E-04 compact timing evidence changed')
    durations = [item['supervision']['duration_s'] for item in source['runs']]
    compact = {
        'schema_version': 1,
        'experiment': 'E-04',
        'task': 'SN-015',
        'date': '2026-09-01',
        'result': 'passed_focused_adc_profile',
        'model_scope': 'owned_experiment_extension_not_complete_stm32f103_adc',
        'complete_local_run': {
            'fresh_renode_processes': 3,
            'native_host_processes': 3,
            'repeated_discrete_results': True,
            'final_virtual_time_us_per_run': [item['final_time_us'] for item in source['runs']],
            'minimum_process_duration_s': min(durations),
            'maximum_process_duration_s': max(durations),
            'total_process_duration_s': sum(durations),
            'firmware_elf_sha256': source['firmware']['elf_sha256'],
            'identical_firmware_rebuild': source['firmware']['identical_rebuild'],
            'native_host_sha256': source['sha256']['e04_probe.exe'],
            'extension_sha256': source['sha256']['stm32f103_adc.cs'],
        },
        'contract': source['profile'],
        'audit': source['audit'],
        'static_points': [
            {'input_uv': row['input_uv'], 'expected_code': row['expected_code'],
             'actual_code': row['actual_code']} for row in run['static']
        ],
        'quantization_and_saturation': [
            {'input_uv': row['input_uv'], 'expected_code': row['expected_code'],
             'actual_code': row['actual_code']} for row in run['boundaries']
        ],
        'channel_mapping': [
            {'channel': row['channel'], 'input_uv': row['input_uv'], 'actual_code': row['actual_code']}
            for row in run['mapping']
        ],
        'ramp': {
            'samples': len(run['ramp']),
            'start_uv': run['ramp'][0]['input_uv'],
            'end_uv': run['ramp'][-1]['input_uv'],
            'step_uv': run['ramp'][1]['input_uv'] - run['ramp'][0]['input_uv'],
            'start_code': run['ramp'][0]['actual_code'],
            'end_code': run['ramp'][-1]['actual_code'],
            'monotonic': run['analysis']['ramp_monotonic'],
            'maximum_code_error': maximum_error,
        },
        'conversion_timing': [
            {'sample_setting': row['setting'], 'duration_us': row['declared_duration_us'],
             'actual_code': row['actual_code']} for row in timing
        ],
        'sample_instant': run['start_snapshot'],
        'adverse_cases': {
            'invalid_channels': run['invalid_channels'],
            'disabled_start': run['disabled_start'],
        },
        'unsupported': [
            'dynamic_vdda_or_vref', 'negative_voltage_api_input', 'electrical_acquisition_effects',
            'scan', 'continuous', 'injected', 'external_trigger', 'interrupt', 'dma',
            'watchdog', 'dual_adc', 'internal_temperature_or_vref_channels', 'general_live_coupling',
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(compact, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
