"""Run Godot lifecycle and optional real-renderer image acceptance."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true', help='Only test extension and lifecycle, not pixels')
    args = parser.parse_args()
    executable = json.loads((ROOT / 'config.local.json').read_text())['godot_executable']
    project = ROOT / 'platform/godot/project'
    options = {'cwd': ROOT, 'capture_output': True, 'text': True, 'timeout': 60}
    if os.name == 'nt':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        options['startupinfo'] = startup
    version = subprocess.check_output([executable, '--version'], text=True).strip()
    if not version.startswith('4.7.2.'):
        raise ValueError('Godot 4.7.2 required, found ' + version)
    imported = subprocess.run([executable, '--headless', '--path', str(project), '--editor', '--import', '--quit'], **options)
    print(imported.stdout, imported.stderr)
    if imported.returncode or 'ERROR:' in imported.stderr:
        return 1
    capture = ROOT / 'build/godot_smoke.png'
    command = [executable, '--path', str(project), '--script', 'res://smoke_test.gd']
    if args.headless:
        command.append('--headless')
    command += ['--', '--capture=' + str(capture)]
    result = subprocess.run(command, **options)
    print(result.stdout, result.stderr)
    if result.returncode or 'ERROR:' in result.stderr or 'leaked' in result.stderr.lower() or 'ROADWARS_SMOKE_OK' not in result.stdout:
        return 1
    if not args.headless:
        print('Verified render capture:', capture)
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
