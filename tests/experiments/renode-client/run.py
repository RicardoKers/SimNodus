"""Run real Renode control separately from bounded scripted-peer fault tests."""
from __future__ import annotations
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import socket
import struct
import subprocess
import sys
import threading
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
HANDSHAKE = struct.pack('<H', 6) + bytes((1, 0, 2, 0, 3, 0, 4, 0, 5, 1, 6, 0))


def check(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def listeners(pid: int) -> list[dict]:
    """Read the actual Windows IPv4 listener table, including process ownership."""
    function = ctypes.WinDLL('iphlpapi').GetExtendedTcpTable
    function.argtypes = [ctypes.c_void_p, ctypes.POINTER(wintypes.DWORD), wintypes.BOOL,
                         wintypes.ULONG, ctypes.c_int, wintypes.ULONG]
    function.restype = wintypes.DWORD
    size = wintypes.DWORD(0)
    code = function(None, ctypes.byref(size), False, socket.AF_INET, 3, 0)
    check(code in (0, 122), 'Cannot size Windows listener table')
    buffer = ctypes.create_string_buffer(size.value)
    check(function(buffer, ctypes.byref(size), False, socket.AF_INET, 3, 0) == 0,
          'Cannot read Windows listener table')
    count = struct.unpack_from('<I', buffer.raw)[0]
    check(4 + count * 24 <= len(buffer), 'Invalid listener table size')
    result = []
    for index in range(count):
        _, address, port, _, _, owner = struct.unpack_from('<6I', buffer.raw, 4 + 24 * index)
        if owner == pid:
            result.append({'address': socket.inet_ntoa(struct.pack('<I', address)),
                           'port': socket.ntohs(port & 0xffff), 'owner_matches': True})
    return result


def invoke(executable: Path, port: int | str, mode: str, directory: Path) -> dict:
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / 'stdout.json').open('w', encoding='utf-8') as output, (directory / 'stderr.log').open('w', encoding='utf-8') as errors:
        process = subprocess.run([str(executable), str(port), mode], stdout=output, stderr=errors,
                                 timeout=15, creationflags=subprocess.CREATE_NO_WINDOW)
    check(process.returncode == 0, f'Native process failed ({process.returncode}); inspect {directory.name} logs')
    return json.loads((directory / 'stdout.json').read_text(encoding='utf-8'))


def real_backend(args, output: Path) -> dict:
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    directory = output / 'real'; directory.mkdir(parents=True, exist_ok=True)
    extension = (args.prepared / 'LoopbackControl.cs').resolve().as_posix()
    check(not any(character.isspace() for character in extension), 'This experiment currently requires a checkout path without whitespace')
    script = f'include @{extension}; mach create "sn019"; emulation CreateLoopbackControlServer "sn019-control" {port}'
    argv = [str(args.renode / 'Renode.exe'), '--disable-xwt', '--console', '--plain',
            '--config', str(directory / 'renode.config'), '--execute', script]
    result = {}
    environment = os.environ.copy()
    temporary = tempfile.mkdtemp(prefix='runtime-', dir=directory)
    environment['TEMP'] = environment['TMP'] = temporary
    log_path = directory / 'renode.log'
    with log_path.open('w', encoding='utf-8') as log:
        process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT,
                                   text=True, env=environment, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                check(process.poll() is None, 'Renode exited before its listener was ready')
                endpoints = listeners(process.pid)
                if endpoints:
                    check(endpoints == [{'address': '127.0.0.1', 'port': port, 'owner_matches': True}],
                          f'Unexpected Renode listening endpoints: {endpoints}')
                    result['listener'] = {'address': '127.0.0.1', 'ephemeral_port': True, 'owner_matches': True}
                    break
                contents = log_path.read_text(encoding='utf-8', errors='replace')
                check('Errors during' not in contents and 'Fatal error' not in contents and 'error executing command' not in contents,
                      'Renode extension/startup failed; inspect log')
                time.sleep(0.05)
            else:
                raise ValueError('Renode listener startup timed out')
            for variant in ('normal', 'fragmented'):
                data = invoke(args.native / f'probe_{variant}.exe', port, 'real', directory / variant)
                check(data['transfer_limit'] == (1 if variant == 'fragmented' else 2147483647), 'Wrong transfer build')
                check(len(data['cycles']) == 3, 'Missing real connection cycles')
                expected_initial = 0 if variant == 'normal' else result['normal']['cycles'][-1]['steps'][-1]['after_us']
                for cycle in data['cycles']:
                    check(cycle['initial_us'] == expected_initial, 'Fresh start or reconnect changed time')
                    check([step['requested_us'] for step in cycle['steps']] == [0, 1, 999, 1000, 1000, 1000000], 'Wrong intervals')
                    for step in cycle['steps']:
                        check(step['after_us'] - step['before_us'] == step['requested_us'], 'Incorrect virtual-time difference')
                    expected_initial = cycle['steps'][-1]['after_us']
                result[variant] = data
        finally:
            if process.poll() is None:
                try:
                    process.communicate('quit\n', timeout=10)
                except (subprocess.TimeoutExpired, OSError):
                    process.kill(); process.communicate(timeout=5)
                    result['forced_cleanup'] = True
            result['process_exit'] = process.returncode
    check(result['process_exit'] == 0 and not result.get('forced_cleanup'), 'Renode did not shut down normally')
    check(not listeners(process.pid), 'Listener survived Renode shutdown')
    check('Errors during' not in log_path.read_text(encoding='utf-8', errors='replace'), 'Renode reported an error')
    result['listener_removed'] = True
    return result


def receive(peer: socket.socket, count: int) -> bytes:
    result = b''
    while len(result) < count:
        chunk = peer.recv(count - len(result))
        if not chunk:
            raise ValueError('Client closed before sending the expected frame')
        result += chunk
    return result


def query(peer: socket.socket) -> None:
    check(receive(peer, 7) == b'RE\x02\x08\x00\x00\x00', 'Time query framing changed')
    check(receive(peer, 8) == bytes(8), 'Time request leaked uninitialized payload')


def fault(executable: Path, name: str, directory: Path) -> dict:
    done = threading.Event(); errors = []
    with socket.socket() as server:
        server.bind(('127.0.0.1', 0)); port = server.getsockname()[1]
        if name == 'refused':
            result = invoke(executable, port, 'connect', directory)
            # Windows may report refusal after our shorter operation deadline.
            check(result['connect_code'] in (0, 4) and result['handle_cleared'] and result['wall_seconds'] < 3,
                  'Unavailable port did not fail within the deadline gate')
            return result
        server.listen(1); server.settimeout(5)

        def serve():
            try:
                for _ in range(21 if name == 'stress' else 1):
                    peer, _ = server.accept()
                    with peer:
                        peer.settimeout(5); peer.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                        check(receive(peer, len(HANDSHAKE)) == HANDSHAKE, 'Pinned handshake changed')
                        if name == 'handshake-close':
                            return
                        if name == 'handshake-timeout':
                            done.wait(3); return
                        if name in ('handshake-invalid', 'stress'):
                            peer.sendall(b'\x7f'); continue
                        peer.sendall(b'\x05')
                        if name == 'invalid-event-run':
                            check(receive(peer, 15) == b'RE\x01' + struct.pack('<IQ', 8, 1), 'RunFor framing changed')
                        else:
                            query(peer)
                        if name == 'response-timeout':
                            done.wait(3); return
                        if name == 'response-close':
                            peer.sendall(b'\x03\x02' + struct.pack('<I', 8) + b'\x00\x00'); return
                        if name == 'slow-response':
                            for byte in b'\x03\x02' + struct.pack('<I', 8) + bytes(8):
                                peer.sendall(bytes([byte]))
                                if done.wait(0.25):
                                    return
                        elif name == 'invalid-code':
                            peer.sendall(b'\xff')
                        elif name == 'wrong-command':
                            peer.sendall(b'\x03\x01' + struct.pack('<I', 8) + bytes(8))
                        elif name == 'oversized-error':
                            peer.sendall(b'\x00\x02' + struct.pack('<I', 0xffffffff))
                        elif name == 'oversized-event':
                            peer.sendall(b'\x06\x05' + struct.pack('<II', 0, 0xffffffff))
                        elif name == 'invalid-event-run':
                            peer.sendall(b'\x06\x05' + struct.pack('<II', 0, 16) + bytes(16))
                        elif name == 'fatal-error':
                            peer.sendall(b'\x01' + struct.pack('<I', 3) + b'bad')
                        elif name in ('short-error', 'empty-error'):
                            message = b'bad' if name == 'short-error' else b''
                            peer.sendall(b'\x00\x02' + struct.pack('<I', len(message)) + message)
                            query(peer)
                            peer.sendall(b'\x03\x02' + struct.pack('<I', 8) + struct.pack('<Q', 123))
                        done.wait(3)
            except (ConnectionResetError, BrokenPipeError):
                pass  # The tested client intentionally retires a failed stream.
            except Exception as error:
                errors.append(str(error))

        thread = threading.Thread(target=serve, daemon=True); thread.start()
        mode = 'stress' if name == 'stress' else ('connect' if name.startswith('handshake') else ('run' if name == 'invalid-event-run' else 'query'))
        try:
            result = invoke(executable, port, mode, directory)
        finally:
            done.set(); thread.join(6)
        check(not thread.is_alive() and not errors, f'Fault peer failed: {errors}')
    if name == 'stress':
        return result
    check(result['handle_cleared'], 'Client retained its handle')
    if name.startswith('handshake'):
        expected = 4 if name.endswith('timeout') else (1 if name.endswith('invalid') else 0)
        check(result['connect_code'] == expected, 'Wrong handshake failure classification')
    else:
        check(result['connect_code'] == -1, 'Fault peer handshake did not finish')
        expected = 4 if name in ('response-timeout', 'slow-response') else (0 if name == 'response-close' else 1)
        if name in ('short-error', 'empty-error'):
            check(result['operation_code'] == 5 and result['reuse_code'] == -1 and result['value_us'] == 123,
                  'Application error ownership/recovery failed')
        else:
            check(result['operation_code'] == expected and result['reuse_code'] == 2, 'Failed stream was not retired')
    check(result['wall_seconds'] < 3, 'Fault handling exceeded wall-time gate')
    if name in ('handshake-timeout', 'response-timeout', 'slow-response'):
        check(0.8 <= result['wall_seconds'], 'Timeout occurred before the declared deadline')
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renode', type=Path, default=ROOT / 'build/deps/renode/renode_1.16.1-dotnet_portable')
    parser.add_argument('--prepared', type=Path, default=ROOT / 'build/sn019/generated')
    parser.add_argument('--native', type=Path, default=ROOT / 'build/sn019/native/Debug')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/sn019/results')
    parser.add_argument('--only', choices=('real', 'faults'))
    args = parser.parse_args()
    for name in ('renode', 'prepared', 'native', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/check_backend_assets.py'), '--renode-only',
                    '--root', str(args.renode.parents[1])], check=True)
    report = {'status': 'passed', 'operation_deadline_ms': 1000, 'cases': {}}
    cases = ['refused', 'handshake-close', 'handshake-timeout', 'handshake-invalid', 'response-close',
             'response-timeout', 'slow-response', 'invalid-code', 'wrong-command', 'oversized-error',
             'oversized-event', 'fatal-error', 'invalid-event-run', 'short-error', 'empty-error', 'stress']
    jobs = []
    if args.only != 'faults':
        jobs.append(('real-renode', lambda: real_backend(args, args.output)))
    if args.only != 'real':
        for name in cases:
            jobs.append((name, lambda name=name: fault(args.native / 'probe_normal.exe', name, args.output / name)))
        def invalid_port(port):
            data = invoke(args.native / 'probe_normal.exe', port, 'connect', args.output / ('invalid-port-' + port))
            check(data['connect_code'] == 1 and data['handle_cleared'], 'Invalid port was accepted')
            return data
        for port in ('0', '65536', 'invalid'):
            jobs.append(('invalid-port-' + port, lambda port=port: invalid_port(port)))
    for name, execute in jobs:
        try:
            result = execute(); result['status'] = 'passed'
        except (ValueError, OSError, subprocess.TimeoutExpired) as error:
            result = {'status': 'failed', 'error': str(error)}; report['status'] = 'failed'
        report['cases'][name] = result
        print(name, result['status'], result.get('error', ''), flush=True)
    paths = [*args.native.glob('probe_*.exe'), *args.prepared.glob('*.c'), *args.prepared.glob('*.h'),
             *args.prepared.glob('*.cs'), *HERE.glob('*.py'), *HERE.glob('*.c'), *HERE.glob('*.cpp'),
             HERE / 'transport.h', HERE / 'CMakeLists.txt', HERE / 'upstream.json']
    report['sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    report['provenance'] = json.loads((args.prepared / 'provenance.json').read_text(encoding='utf-8'))
    (args.output / 'summary.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
