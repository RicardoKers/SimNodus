"""Explicit owned-project SN-012 acceptance; Python retains fixture/process control."""
import argparse
from contextlib import contextmanager, ExitStack
import ctypes
from ctypes import wintypes
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'tests/schema'))
sys.path.insert(0, str(ROOT / 'tests/resources'))
import native_target_regression as target
import local_resources as physical
spec = importlib.util.spec_from_file_location('e02_project_acceptance', HERE / 'run.py')
e02 = importlib.util.module_from_spec(spec);spec.loader.exec_module(e02)


@contextmanager
def pinned_stage(root, files):
    """Experiment-only lease: retain every ancestor and checked file until exit."""
    fs = physical.WindowsFiles();drive, parts = physical.root_parts(str(root))
    with ExitStack() as stack:
        current = fs.volume(drive, stack)
        for part in parts:current, _ = fs.child(current, part, True, stack, '$stage')
        for name, expected in files.items():
            handle, info = fs.child(current, name, False, stack, name)
            assert info.size() == len(expected)
            chunks = bytearray()
            while len(chunks) <= len(expected):
                data = fs.read(handle, min(65536, len(expected) + 1 - len(chunks)), name)
                if not data:break
                chunks.extend(data)
            assert bytes(chunks) == expected, name
        # Derive a volume-GUID spelling from the retained directory handle.
        # Containment comes from handle-relative traversal, never this string.
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
    args = parser.parse_args();output = args.output.resolve();output.mkdir(parents=True, exist_ok=False)
    report = {'status': 'started', 'profiles': {}, 'execution': 'explicit owned SN-012 experiment only'}
    try:
        with (output / 'assets.log').open('w', encoding='utf-8') as log:
            subprocess.run([sys.executable, str(ROOT / 'tools/check_backend_assets.py'), '--renode-only'],
                           cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        source = ROOT / 'build/sn012/firmware/firmware.elf'
        record = json.loads((source.parent / 'build.json').read_bytes())
        image = source.read_bytes();assert hashlib.sha256(image).hexdigest() == record['elf_sha256']
        for name, digest in record['sources_sha256'].items():assert e02.sha(HERE / name) == digest, name
        prepared = ROOT / 'build/sn019/generated'
        provenance = json.loads((prepared / 'provenance.json').read_bytes())
        for name, digest in provenance.items():
            if name.startswith('generated/'):assert e02.sha(prepared / Path(name).name) == digest, name
        control = (prepared / 'LoopbackControl.cs').read_bytes()
        package = output / 'package';doc = target.package(package, image)
        (output / 'project.json').write_bytes((json.dumps(doc, indent=2) + '\n').encode())
        target.PROBE = str(ROOT / 'build/sn021-save/Debug/native_target_probe.exe')
        def replace_sources():
            (package / 'firmware.elf').write_bytes(b'replaced firmware')
            (package / 'platform.repl').write_bytes(b'replaced platform')
        inspected = target.inspect(doc, package, after_capture=replace_sources)
        assert 'error' not in inspected, inspected
        assert inspected['firmware'] == image and inspected['platform'] == target.PLATFORM.read_bytes()
        report['boot'] = inspected['boot'];report['source_replacement_preserved_snapshots'] = True
        rejected = target.inspect(doc, package);assert 'error' in rejected
        report['reinspection_after_replacement'] = rejected
        staged = output / 'stage';staged.mkdir()
        files = {'firmware.elf': inspected['firmware'], 'stm32f103c8.repl': inspected['platform'], 'LoopbackControl.cs': control}
        for name, data in files.items():(staged / name).write_bytes(data)
        with pinned_stage(staged, files) as pinned:
            denied = {}
            for name in files:
                try:(staged / name).write_bytes(b'changed')
                except OSError as error:denied[name] = error.winerror
                else:raise AssertionError('Pinned file modification was permitted')
            try:staged.rename(staged.with_name('moved-stage'))
            except OSError as error:denied['directory-rename'] = error.winerror
            else:raise AssertionError('Pinned directory rename was permitted')
            report['staging_changes_denied'] = denied
            # Reuse the unchanged engine orchestration and all E-02 assertions.
            e02.HERE = pinned
            run_args = SimpleNamespace(renode=ROOT / 'build/deps/renode/renode_1.16.1-dotnet_portable',
                prepared=pinned, firmware=pinned, native=ROOT / 'build/sn012/native/Debug', output=output / 'engine')
            for step in (100, 1000):report['profiles'][str(step)] = e02.run(run_args, step)
        report['status'] = 'passed'
    except Exception as error:
        report['status'] = 'failed';report['error'] = str(error);raise
    finally:
        report['files_sha256'] = {p.relative_to(output).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in output.rglob('*') if p.is_file() and 'runtime-' not in p.as_posix()}
        (output / 'result.json').write_bytes((json.dumps(report, indent=2) + '\n').encode())
        print(json.dumps({'status': report['status'], 'error': report.get('error'),
                          'profiles': {k: v.get('status') for k, v in report['profiles'].items()}}))


if __name__ == '__main__':main()
