"""Verify pinned sources and generate the explicit Windows/loopback experiment variants."""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def replace(text: str, before: str, after: str, count: int = 1) -> str:
    if text.count(before) != count:
        raise ValueError(f"Unexpected upstream text: {before[:70]!r}")
    return text.replace(before, after)


def section(text: str, start: str, end: str, content: str) -> str:
    a, b = text.index(start), text.index(end, text.index(start))
    return text[:a] + content + "\n\n" + text[b:]


def client(text: str) -> str:
    for name in ('fcntl.h', 'netdb.h', 'sys/socket.h', 'sys/types.h', 'unistd.h'):
        text = replace(text, f'#include <{name}>\n', '')
    text = replace(text, '#include "renode_api.h"', '#include "renode_api.h"\n#include "transport.h"')
    text = text.replace('int socket_fd', 'sn_socket *socket_fd')
    text = replace(text, '__builtin_expect(!!(x), 0)', '!!(x)')
    text = section(text, 'static void xcleanup(', 'static renode_error_t *create_error_static', '')
    text = section(text, 'static renode_error_t *write_or_fail(', 'static renode_error_t *perform_handshake(', r'''
static renode_error_t *io_error(sn_result result)
{
    switch (result) {
    case SN_OK: return NO_ERROR;
    case SN_TIMEOUT: return create_error_static(ERR_TIMEOUT, "Socket operation deadline expired");
    case SN_NOT_CONNECTED: return create_error_static(ERR_NOT_CONNECTED, "Connection is unusable; reconnect explicitly");
    case SN_INVALID: return create_fatal_error_static("Invalid loopback port");
    default: return create_error_static(ERR_CONNECTION_FAILED, "Loopback connection failed or closed");
    }
}
static renode_error_t *protocol_error(renode_t *renode, char *message)
{
    sn_abort(renode->socket_fd);
    return create_fatal_error_static(message);
}
static renode_error_t *write_or_fail(sn_socket *socket_fd, const uint8_t *data, size_t count)
{
    return io_error(sn_send_all(socket_fd, data, count));
}
static renode_error_t *read_byte_or_fail(sn_socket *socket_fd, uint8_t *value)
{
    return io_error(sn_receive_all(socket_fd, value, 1));
}
static renode_error_t *read_or_fail(sn_socket *socket_fd, uint8_t *buffer, uint32_t count)
{
    return io_error(sn_receive_all(socket_fd, buffer, count));
}''')
    text = section(text, 'static renode_error_t *obtain_socket(', 'static renode_error_t *renode_send_header(', r'''
renode_error_t *renode_connect(const char *port, renode_t **renode)
{
    assert(port != NULL && renode != NULL);
    *renode = NULL;
    sn_socket *socket_fd = NULL;
    return_error_if_fails(io_error(sn_open(port, &socket_fd)));
    renode_error_t *error = perform_handshake(socket_fd);
    if (error) { sn_close(socket_fd); return error; }
    *renode = xmalloc(sizeof(renode_t));
    (*renode)->socket_fd = socket_fd;
    return NO_ERROR;
}
renode_error_t *renode_disconnect(renode_t **renode)
{
    assert(renode != NULL && *renode != NULL);
    sn_close((*renode)->socket_fd);
    free(*renode);
    *renode = NULL;
    return NO_ERROR;
}''')
    # MSVC does not implement GNU cleanup attributes. Preserve all early-return frees.
    cleanup_macros = r'''
#define cleanup_call(call, ptr) do { renode_error_t *error = (call); if (error) { free(ptr); return error; } } while (0)
#define cleanup_assert(test, message, ptr, connection) do { if (!(test)) { free(ptr); return protocol_error(connection, message); } } while (0)
'''
    text = replace(text, 'renode_error_t *renode_get_machine(', cleanup_macros + '\nrenode_error_t *renode_get_machine(')
    for start, end, pointer, connection in [
        ('renode_error_t *renode_get_machine(', 'static renode_error_t *renode_get_instance_descriptor(', 'data', 'renode'),
        ('static renode_error_t *renode_get_instance_descriptor(', 'struct __attribute__((packed)) run_for_out', 'data', 'machine->renode'),
        ('renode_error_t *renode_sysbus_read(', 'renode_error_t *renode_sysbus_write(', 'command', 'ctx->machine->renode'),
        ('renode_error_t *renode_sysbus_write(', None, 'command', 'ctx->machine->renode')]:
        a = text.index(start); b = text.index(end, a) if end else len(text)
        part = text[a:b].replace('__attribute__ ((__cleanup__(xcleanup)))', '')
        # Only statements after the allocation own this buffer.
        split = part.index('= xmalloc('); split = part.index(';', split) + 1
        prefix, tail = part[:split], part[split:]
        tail = re.sub(r'return_error_if_fails\(([^\n]+)\);', rf'cleanup_call(\1, {pointer});', tail)
        tail = re.sub(r'assert_msg\(([^\n]+)\);', rf'cleanup_assert(\1, {pointer}, {connection});', tail)
        tail = tail.replace('return NO_ERROR;', f'free({pointer});\n    return NO_ERROR;')
        text = text[:a] + prefix + tail + text[b:]
    # Preserve wire layouts without changing the public header's natural alignment.
    text = replace(text, 'struct __attribute__((packed)) run_for_out {', '#pragma pack(push, 1)\nstruct run_for_out {')
    text = replace(text, '    uint64_t microseconds;\n};', '    uint64_t microseconds;\n};\n#pragma pack(pop)\n_Static_assert(sizeof(struct run_for_out) == 15, "RunFor wire size");')
    text = replace(text, 'typedef union {\n    struct {', 'typedef union {\n#pragma pack(push, 1)\n    struct {', 2)
    text = replace(text, '    } __attribute__((packed)) out;', '    } out;\n#pragma pack(pop)', 2)
    text = replace(text, 'struct __attribute__((packed)) event_gpio_frame', '#pragma pack(push, 1)\nstruct event_gpio_frame')
    text = replace(text, '    int32_t ed;\n};', '    int32_t ed;\n};\n#pragma pack(pop)')
    text = replace(text, 'typedef struct __attribute__((packed)) {', '#pragma pack(push, 1)\ntypedef struct {')
    text = replace(text, '} sysbus_command_t;', '} sysbus_command_t;\n#pragma pack(pop)')
    # Error text must be owned even when an original command used a stack buffer.
    start = text.index('            if (buffer_size < *data_size + 1)')
    end = text.index('            break;', start)
    text = text[:start] + r'''
            if (*data_size > 1048576) { return protocol_error(renode, "Error payload exceeds experiment limit"); }
            buffer = xmalloc((size_t)*data_size + 1);
            buffer[*data_size] = '\0';
            renode_error_t *read_error = renode_receive_bytes(renode, buffer, *data_size);
            if (read_error) { free(buffer); return read_error; }
''' + text[end:]
    text = replace(text, '    uint8_t received_command;', '    uint8_t received_command = 0;')
    text = replace(text, '        case FATAL_ERROR:\n            error_code = ERR_COMMAND_FAILED;',
                   '        case FATAL_ERROR:\n            sn_abort(renode->socket_fd);\n            error_code = ERR_FATAL;')
    text = replace(text, '    struct renode_event *event = xmalloc',
                   '    if (size > 1048576) { return protocol_error(renode, "Event payload exceeds experiment limit"); }\n\n    struct renode_event *event = xmalloc')
    # Malformed frames invalidate this stream; application command errors need not.
    a, b = text.index('static renode_error_t *renode_receive_response('), text.index('static renode_error_t *renode_execute_command(')
    part = text[a:b].replace('create_fatal_error_static(', 'protocol_error(renode, ')
    text = text[:a] + part + text[b:]
    text = replace(text, '    assert(api_command != ANY_COMMAND && api_command != EVENT);',
                   '    assert(renode != NULL);\n    return_error_if_fails(io_error(sn_begin(renode->socket_fd)));\n    assert(api_command != ANY_COMMAND && api_command != EVENT);')
    text = replace(text, '    assert(renode != NULL && value < UINT64_MAX / unit);',
                   '    assert(renode != NULL);\n    assert(unit == TU_MICROSECONDS || unit == TU_MILLISECONDS || unit == TU_SECONDS);\n    assert(value <= UINT64_MAX / (uint64_t)unit);\n    return_error_if_fails(io_error(sn_begin(renode->socket_fd)));')
    text = replace(text, '        if (command == RUN_FOR) {\n            break;',
                   '        if (command == RUN_FOR) {\n            if (response_size != 0) { return protocol_error(renode, ERRMSG_UNEXPECTED_RESPONSE_PAYLOAD_SIZE); }\n            break;')
    text = replace(text, '            return create_fatal_error_static(ERRMSG_COMMAND_MISMATCH);',
                   '            return protocol_error(renode, ERRMSG_COMMAND_MISMATCH);')
    text = replace(text, '        if (error != NO_ERROR) {\n            return error;',
                   '        if (error != NO_ERROR) {\n            sn_abort(renode->socket_fd);\n            return error;')
    text = replace(text, '    assert(renode != NULL);\n\n    uint64_t divider;',
                   '    assert(renode != NULL && current_time != NULL);\n    *current_time = 0;\n\n    uint64_t divider;')
    text = replace(text, '    assert_msg(response_size == sizeof(*current_time), ERRMSG_UNEXPECTED_RESPONSE_PAYLOAD_SIZE);',
                   '    if (response_size != sizeof(*current_time)) { return protocol_error(renode, ERRMSG_UNEXPECTED_RESPONSE_PAYLOAD_SIZE); }')
    text = replace(text, '    uint32_t name_length = strlen(name);',
                   '    assert(name != NULL && strlen(name) <= 1048576);\n    uint32_t name_length = (uint32_t)strlen(name);', 2)
    text = replace(text, '    uint32_t result;\n    switch (width)', '    uint64_t result;\n    switch (width)')
    text = replace(text, '        result = (uint32_t)width * count;', '        result = (uint64_t)width * count;')
    text = replace(text, '    size_t payload_size = sizeof(sysbus_command_t) + data_bytes;',
                   '    assert(data_bytes <= 1048576);\n    uint32_t payload_size = (uint32_t)(sizeof(sysbus_command_t) + data_bytes);', 2)
    return text


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT / 'build/sn019/source')
    parser.add_argument('--renode', type=Path, default=ROOT / 'build/deps/renode/renode_1.16.1-dotnet_portable')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/sn019/generated')
    parser.add_argument('--download', action='store_true', help='Explicitly download only the pinned small audit/extension sources')
    args = parser.parse_args()
    manifest = json.loads((HERE / 'upstream.json').read_text(encoding='utf-8'))
    args.source_root.mkdir(parents=True, exist_ok=True)
    fingerprints = {}
    for entry in manifest['files']:
        path = args.source_root / entry['file']
        if args.download:
            with urllib.request.urlopen(entry['url'], timeout=30) as response:
                data = response.read(1024 * 1024)
        else:
            data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != entry['sha256']:
            raise ValueError(f"Source hash mismatch: {entry['file']}")
        if args.download:
            path.write_bytes(data)
        fingerprints[entry['file']] = digest
    baseline = json.loads((ROOT / 'tools/backend-baseline.json').read_text(encoding='utf-8'))
    for entry in baseline['files']:
        prefix = 'renode/renode_1.16.1-dotnet_portable/'
        if not entry['path'].startswith(prefix):
            continue
        path = args.renode / entry['path'][len(prefix):]
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != entry['sha256']:
            raise ValueError(f'Package file hash mismatch: {path.name}')
        fingerprints[path.name] = digest
    args.output.mkdir(parents=True, exist_ok=True)
    source = (args.renode / 'tools/external_control_client/lib/renode_api.c').read_text(encoding='utf-8')
    adapted = client(source)
    (args.output / 'renode_api.c').write_text(adapted, encoding='utf-8', newline='\n')
    header = (args.renode / 'tools/external_control_client/include/renode_api.h').read_bytes()
    (args.output / 'renode_api.h').write_bytes(header)
    units = []
    for entry in manifest['files']:
        name = entry['file']
        if not name.endswith('.cs') or name == 'SocketsManager.cs':
            continue
        text = (args.source_root / name).read_text(encoding='utf-8')
        text = text.replace('Antmicro.Renode.Network', 'SimNodus.Experimental.Network')
        text = text.replace('SocketServerProvider', 'SimNodusSocketServerProvider')
        text = text.replace('CreateExternalControlServer', 'CreateLoopbackControlServer')
        if name == 'SocketServerProvider.cs':
            text = replace(text, 'IPAddress.Any', 'IPAddress.Loopback')
        imports = re.findall(r'^using [^\n]+;', text, re.M) + ['using Antmicro.Renode;']
        text = re.sub(r'^using [^\n]+;\r?\n', '', text, flags=re.M)
        text = re.sub(r'(namespace [^\n]+\n\{)', lambda m: m[1]+'\n'+'\n'.join(imports), text, count=1)
        units.append(text)
    (args.output / 'LoopbackControl.cs').write_text('\n'.join(units), encoding='utf-8', newline='\n')
    licenses = args.output / 'licenses'; licenses.mkdir(exist_ok=True)
    (licenses / 'MIT.txt').write_bytes((HERE / 'UPSTREAM-LICENSE.txt').read_bytes())
    fingerprints.update({'generated/' + p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in args.output.iterdir() if p.is_file() and p.suffix in ('.c', '.h', '.cs')})
    (args.output / 'provenance.json').write_text(json.dumps(fingerprints, indent=2)+'\n', encoding='utf-8')
    print('Verified sources; generated native client and loopback extension. No server was started.')


if __name__ == '__main__':
    main()
