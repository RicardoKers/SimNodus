"""Read-only ELF inspection against an existing owned firmware build record."""
import argparse
import hashlib
import json
from pathlib import Path
import native_elf_regression as elf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', required=True, type=Path)
    parser.add_argument('--image', required=True, type=Path)
    parser.add_argument('--build-record', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    # Reserve a new evidence file before attempting any inspection.
    with args.output.open('x', encoding='utf-8') as output:
        report = {'status': 'started', 'engine_run': False, 'firmware_loaded': False}
        try:
            raw = args.image.read_bytes();record_bytes = args.build_record.read_bytes()
            record = json.loads(record_bytes)
            report['elf_sha256'] = hashlib.sha256(raw).hexdigest()
            report['build_record_sha256'] = hashlib.sha256(record_bytes).hexdigest()
            report['probe_sha256'] = hashlib.sha256(args.probe.read_bytes()).hexdigest()
            report['compiler_version'] = record['compiler_version']
            assert report['elf_sha256'] == record['elf_sha256'], 'Historical build digest mismatch'
            elf.PROBE = str(args.probe)
            report['inspection'] = elf.inspect(raw)
            assert 'error' not in report['inspection'], report['inspection']
            assert report['inspection'] == elf.oracle(raw), 'Binary field comparison failed'
            report['status'] = 'passed'
        except Exception as error:
            report['status'] = 'failed';report['error'] = str(error);raise
        finally:
            output.write(json.dumps(report, indent=2) + '\n')
            print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
