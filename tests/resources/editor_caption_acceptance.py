# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Independent exact captions layered on the unchanged compact C acceptance."""
import argparse
from decimal import Decimal, localcontext
import hashlib
import json
from pathlib import Path
import re

import editor_circuit_capacitance_acceptance as circuit_acceptance

FIXED_DECIMAL = re.compile(r'(?:0|[1-9][0-9]*)(?:\.[0-9]+)?\Z')
REQUIRED_CASES = [
    ('1', 'ohm'),
    ('999', 'ohm'),
    ('1000', 'ohm'),
    ('2200.000', 'ohm'),
    ('999999.999999999999999999999999', 'ohm'),
    ('1000000', 'ohm'),
    ('999999999', 'ohm'),
    ('1000000000', 'ohm'),
    ('0.1', 'ohm'),
    ('0.000000001', 'F'),
    ('0.000000999', 'F'),
    ('0.000001', 'F'),
    ('0.000999999', 'F'),
    ('0.001', 'F'),
    ('0.999', 'F'),
    ('1', 'F'),
    ('0.000000000999', 'F'),
    ('0.000000220', 'F'),
    ('0.0000002200000000000000001', 'F'),
    ('0', 'F'),
    ('-0.000001', 'F'),
    ('1e-6', 'F'),
    ('001', 'ohm'),
    ('.', 'F'),
    ('12x', 'ohm'),
    ('1', 's'),
    ('0,000001', 'F'),
    ('2200.' + '0' * 80, 'ohm'),
    ('1' * 129, 'ohm'),
]


def expected_caption(value, unit):
    """Use Decimal arithmetic, independently of the Qt text-shift algorithm."""
    fallback = value + ' ' + unit
    if len(value) > 128 or FIXED_DECIMAL.fullmatch(value) is None:
        return fallback
    with localcontext() as context:
        context.prec = 200
        number = Decimal(value)
        if unit == 'ohm' and Decimal('1') <= number < Decimal('1000000000'):
            exponent, suffix = (6, 'MΩ') if number >= Decimal('1000000') else (
                (3, 'kΩ') if number >= Decimal('1000') else (0, 'Ω'))
        elif unit == 'F' and Decimal('0.000000001') <= number < Decimal('1'):
            exponent, suffix = (-3, 'mF') if number >= Decimal('0.001') else (
                (-6, 'µF') if number >= Decimal('0.000001') else (-9, 'nF'))
        else:
            return fallback
        mantissa = number.scaleb(-exponent)
        assert Decimal('1') <= mantissa < Decimal('1000')
        text = format(mantissa, 'f')
        if '.' in text:
            text = text.rstrip('0').rstrip('.')
        return text + ' ' + suffix


def verify_cases(cases, audit):
    actual_inputs = [(row['value'], row['unit']) for row in cases]
    assert actual_inputs == REQUIRED_CASES, 'Missing, duplicated, reordered or unexpected caption cases'
    assert len(actual_inputs) == len(set(actual_inputs)), 'Caption case inputs must be distinct'
    for row in cases:
        assert set(row) == {'value', 'unit', 'caption'}, row
        assert all(type(row[key]) is str for key in row), row
        expected = expected_caption(row['value'], row['unit'])
        audit['cases'].append(dict(value=row['value'], unit=row['unit'], expected=expected, actual=row['caption']))
        assert row['caption'] == expected, row


def verify_observations(observations, audit):
    assert len(observations) == 14, 'The unchanged C path must retain all fourteen observations'
    for observation in observations:
        canvas = observation['canvas']
        properties = observation['properties_text']
        assert type(properties) is str, observation
        components = canvas['components']
        assert len(components) == (2 if canvas['supported'] else 0), observation
        for component in components:
            assert component['component'] in ('resistor', 'capacitor'), component
            parameter_id = 'resistance' if component['component'] == 'resistor' else 'capacitance'
            parameter = component['parameters'][parameter_id]
            expected = expected_caption(parameter['value'], parameter['unit'])
            audit['components'].append(dict(stage=observation['stage'], path=component['path'],
                value=parameter['value'], unit=parameter['unit'], expected=expected, actual=component['caption']))
            assert component['caption'] == expected, component
        selected = canvas['selected_path']
        if not selected:
            assert 'Applied ' not in properties and 'Base value:' not in properties, observation
            continue
        matching = [component for component in components if component['path'] == selected]
        assert len(matching) == 1, observation
        component = matching[0]
        parameter_id = 'resistance' if component['component'] == 'resistor' else 'capacitance'
        parameter = component['parameters'][parameter_id]
        expected = expected_caption(parameter['value'], parameter['unit'])
        base = 'Base value: ' + parameter['value'] + ' ' + parameter['unit']
        block = f'Applied {parameter_id}:\n{expected}\n{base}'
        audit['properties'].append(dict(stage=observation['stage'], path=selected,
            expected_block=block, actual_text=properties))
        assert block in properties, observation
        assert [line for line in properties.splitlines() if line.startswith('Base value:')] == [base], observation
    assert len(audit['components']) == 26, 'Thirteen supported snapshots must expose both actual painted captions'
    assert len(audit['properties']) == 12, 'Every selected supported snapshot must retain caption and exact base value'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--editor', required=True)
    parser.add_argument('--qt-kit', required=True)
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    out = Path(args.out).resolve()
    if out.exists():
        raise FileExistsError(f'Refusing to reuse acceptance output directory: {out}')
    audit = dict(state='caption-audit-requested', cases=[], components=[], properties=[])
    raw_retained = False

    def retain_raw_report():
        nonlocal raw_retained
        report = out / 'report.json'
        if raw_retained or not report.is_file():
            return
        raw = report.read_bytes()
        with (out / 'captions-report.json').open('xb') as destination:
            destination.write(raw)
        audit['raw_report_sha256'] = hashlib.sha256(raw).hexdigest()
        raw_retained = True

    try:
        circuit_acceptance.main()
        audit['existing_41_controls_and_native_saved_token_audit'] = 'passed'
        retain_raw_report()
        observed = json.loads((out / 'captions-report.json').read_bytes())
        verify_cases(observed['caption_cases'], audit)
        # The unchanged driver first independently verifies all native snapshots.
        verify_observations(observed['observations'], audit)
        image = out / 'report.json.left.png'
        assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), image
        audit.update(state='passed', case_count=len(audit['cases']), component_count=len(audit['components']),
            properties_count=len(audit['properties']), left_png='passed')
    except Exception as error:
        audit.update(state='failed', error=f'{type(error).__name__}: {error}')
        retain_raw_report()
        raise
    finally:
        if out.is_dir():
            with (out / 'captions-audit.json').open('x', encoding='utf-8') as destination:
                destination.write(json.dumps(audit, ensure_ascii=False, indent=2) + '\n')
    print(f'PASS {len(REQUIRED_CASES)} independent exact-caption cases, 26 component captions, '
          '12 selected Properties/base-value blocks and a fourth PNG; unchanged 41-control C path reused')


if __name__ == '__main__':
    main()
