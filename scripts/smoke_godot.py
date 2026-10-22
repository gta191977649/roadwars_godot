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
    parser.add_argument('--fog', action='store_true', help='Render original linear fog and exempt-material fixtures')
    args = parser.parse_args()
    if args.fog and args.headless:
        parser.error('--fog requires a real rendering surface')
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
    capture = ROOT / ('build/fog_smoke.png' if args.fog else 'build/godot_smoke.png')
    command = [executable, '--path', str(project), '--script', 'res://' + ('fog_smoke_test.gd' if args.fog else 'smoke_test.gd')]
    if args.headless:
        command.append('--headless')
    command += ['--', '--diagnostic', '--no-debug-overlay', '--capture=' + str(capture)]
    log_path = ROOT / 'build' / ('fog_smoke.log' if args.fog else 'godot_smoke.log')
    log_path.unlink(missing_ok=True)
    command[1:1] = ['--log-file', str(log_path)]
    result = subprocess.run(command, **options)
    log = log_path.read_text(encoding='utf-8', errors='replace') if log_path.is_file() else ''
    output = result.stdout + '\n' + result.stderr + '\n' + log
    print(output)
    if not args.headless and not any('Vulkan ' in line and ' - Forward+ - ' in line
                                     for line in output.splitlines()):
        print('Expected the default Vulkan Forward+ renderer', file=sys.stderr)
        return 1
    marker = 'ROADWARS_FOG_SMOKE_OK' if args.fog else 'ROADWARS_SMOKE_OK'
    if result.returncode or 'ERROR:' in output or 'leaked' in output.lower() or marker not in output:
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
