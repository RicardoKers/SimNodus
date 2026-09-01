"""Verify the exact read-only ADC audit sources and the findings used by E-04."""
from __future__ import annotations

import argparse
import hashlib
import json
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require(text: str, fragment: str, finding: str) -> None:
    if fragment not in text:
        raise ValueError(f'Audit finding changed: {finding}')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT / 'build/sn015/audit')
    parser.add_argument('--download', action='store_true')
    args = parser.parse_args()
    args.root.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((HERE / 'audit-sources.json').read_text(encoding='utf-8'))
    fingerprints = {}
    contents = {}
    for entry in manifest['sources']:
        path = args.root / entry['file']
        if args.download:
            with urllib.request.urlopen(entry['url'], timeout=30) as response:
                data = response.read(2 * 1024 * 1024)
            path.write_bytes(data)
        else:
            data = path.read_bytes()
        actual = digest(data)
        if actual != entry['sha256']:
            raise ValueError(f"Source fingerprint mismatch: {entry['file']}")
        fingerprints[entry['file']] = actual
        contents[entry['file']] = data.decode('utf-8-sig')

    generic = contents['STM32_ADC.cs']
    require(generic, 'public class STM32_ADC : BasicDoubleWordPeripheral, IKnownSize',
            'the generic model does not implement IADC')
    require(generic, '.WithFlag(30,', 'the generic software-start bit is 30')
    require(generic, 'public void FeedSample(uint value, uint channelIdx',
            'the generic model accepts pre-quantized raw codes')
    iadc = contents['IADC.cs']
    require(iadc, '@this.SetADCValue(channel, (uint)(value * 1e6m));',
            'IADC voltage setters convert volts to integer microvolts')
    require(iadc, 'uint GetADCValue(int channel);', 'IADC values are unsigned integers')
    header = contents['stm32f103xb.h']
    for fragment, finding in (
        ('#define ADC_CR2_SWSTART_Pos                 (22U)', 'F103 SWSTART is bit 22'),
        ('#define ADC_CR2_EXTTRIG_Pos                 (20U)', 'F103 EXTTRIG is bit 20'),
        ('#define ADC_SQR3_SQ1_Pos                    (0U)', 'first regular rank starts at bit 0'),
        ('#define ADC1_BASE             (APB2PERIPH_BASE + 0x00002400UL)', 'ADC1 base offset is 0x2400')):
        require(header, fragment, finding)
    hal_header = contents['stm32f1xx_hal_adc.h']
    require(hal_header, 'Conversion time is the addition of sampling time and processing time (12.5 ADC clock cycles',
            'F1 conversion adds 12.5 ADC clock cycles')
    require(hal_header, 'ADC_SAMPLETIME_239CYCLES_5', 'all eight F1 sampling selections are declared')
    hal_source = contents['stm32f1xx_hal_adc.c']
    require(hal_source, 'ADC_CR2_SWSTART | ADC_CR2_EXTTRIG',
            'the official software-start route sets SWSTART and EXTTRIG')

    result = {
        'status': 'passed',
        'fingerprints': fingerprints,
        'findings': {
            'generic_model_selected': False,
            'generic_model_implements_iadc': False,
            'generic_model_swstart_bit': 30,
            'stm32f103_swstart_bit': 22,
            'iadc_unit': 'integer_microvolts',
            'conversion_processing_cycles': 12.5,
        },
    }
    (args.root / 'audit.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
