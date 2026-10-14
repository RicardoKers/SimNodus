# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Prepare an isolated owned fixture and optionally run the bounded Windows editor."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path, help='New demo directory; existing directories are refused')
    parser.add_argument('--prepare-only', action='store_true', help='Copy inputs without requiring Qt or launching an editor')
    parser.add_argument('--editor', type=Path, help='Existing locally built simnodus_document_editor.exe')
    parser.add_argument('--qt-kit', type=Path, help='Existing compatible Qt MSVC kit directory')
    args = parser.parse_args()
    out = args.out.resolve()
    if out.exists():
        parser.error('Output directory already exists; choose a new directory.')

    editor = kit = None
    if not args.prepare_only:
        if sys.platform != 'win32':
            parser.error('Interactive launch requires Windows; use --prepare-only elsewhere.')
        if args.editor is None or args.qt_kit is None:
            parser.error('Interactive launch requires --editor and --qt-kit.')
        editor, kit = args.editor.resolve(), args.qt_kit.resolve()
        if not editor.is_file():
            parser.error('Editor executable is unavailable: ' + str(editor))
        for relative in ('bin/Qt6Core.dll', 'bin/Qt6Gui.dll', 'bin/Qt6Widgets.dll', 'plugins/platforms/qwindows.dll'):
            if not (kit / relative).is_file():
                parser.error('Qt runtime prerequisite is unavailable: ' + str(kit / relative))

    fixture = Path(__file__).resolve().parents[1] / 'tests/schema/fixtures'
    # Read only owned repository inputs before creating anything. Resource lookup
    # in the editor remains an independently requested native Preview operation.
    document = (fixture / 'two-rc-project.json').read_bytes()
    artwork = (fixture / 'assets/passive.svg').read_bytes()
    out.mkdir(parents=True, exist_ok=False)
    root = out / 'document'
    resource_root = out / 'explicit-resource-root'
    root.mkdir()
    (root / 'original.json').write_bytes(document)
    asset = resource_root / 'tests/schema/fixtures/assets/passive.svg'
    asset.parent.mkdir(parents=True)
    asset.write_bytes(artwork)
    instructions = (
        'SimNodus bounded editor demo\n\n'
        '1. File -> Open: ' + str(root / 'original.json') + '\n'
        '2. Components: select resistor, then Preview Fixture Artwork...\n'
        '   Choose this separate resource root: ' + str(resource_root) + '\n'
        '3. If this build has Occurrence (read only), select main/right/r there\n'
        '   and Capture Occurrence Artwork... using the explicit root above.\n'
        '   Its selection/capture is independent of Components and Properties.\n'
        '4. Structure: select Instance: right under Circuit: main.\n'
        '   In Instance Properties, change resistance from 2.2 to 3.5 kohm,\n'
        '   then Apply Resistance Value. Applied values appear in the Inspector.\n'
        '5. Edit -> Undo Last Edit / Redo Last Edit offers one step.\n'
        '6. File -> Save Copy: edited.json. Explicitly Open that copy to inspect it.\n'
        '7. View -> Open Signal Analyzer opens the independent, currently empty window.\n\n'
        'Apply or restore pending text fields before Save Copy or history actions.\n'
        'This demo has no simulation, wiring, drag placement or production Save.\n'
        'Close the editor to finish the launcher. Human usability, DPI and packaging\n'
        'remain pending; no Qt binaries, model files or licenses are copied.\n'
    )
    (out / 'INSTRUCTIONS.txt').write_text(instructions, encoding='utf-8')
    print(instructions, flush=True)
    if args.prepare_only:
        return 0

    env = dict(os.environ)
    env['PATH'] = str(kit / 'bin') + os.pathsep + env.get('PATH', '')
    env['QT_QPA_PLATFORM'] = 'windows'
    env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(kit / 'plugins/platforms')
    command = [str(editor)]  # Blank interactive window; Open/Preview remain explicit.
    record = {'command': command, 'document_root': str(root), 'resource_root': str(resource_root),
              'qt_kit': str(kit), 'state': 'launch-requested', 'window_visibility': 'unverified'}

    def save_record():
        (out / 'launch.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')

    save_record()
    try:
        with (out / 'editor.stdout.log').open('xb') as stdout, (out / 'editor.stderr.log').open('xb') as stderr:
            process = subprocess.Popen(command, env=env, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr)
            record.update(state='process-started', pid=process.pid)
            save_record()
            print('Editor process started. Close its window to finish; diagnostics: ' + str(out), flush=True)
            result = process.wait()
    except OSError as error:
        record.update(state='launch-failed', error=str(error))
        save_record()
        print('Editor launch failed; diagnostics: ' + str(out), file=sys.stderr)
        return 1
    record.update(state='process-exited', exit_code=result)
    save_record()
    if result != 0:
        print('Editor exited with code ' + str(result) + '; diagnostics: ' + str(out), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
