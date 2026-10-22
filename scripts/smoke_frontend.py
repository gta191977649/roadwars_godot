"""Verify original frontend startup, screen rendering and host lifetime."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from local_config import game_directory

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--races', action='store_true', help='Check live Single/Quick menu launches, Loading and BGM')
    parser.add_argument('--default-config', action='store_true', help='Exercise the no-game-dir host config path')
    args = parser.parse_args()
    config = json.loads((ROOT / 'config.local.json').read_text())
    capture_dir = ROOT / 'build' / 'frontend_captures'
    capture_dir.mkdir(parents=True, exist_ok=True)
    command = [config['godot_executable'], '--path', str(ROOT / 'platform/godot/project'),
               '--script', 'res://race_menu_smoke_test.gd' if args.races else 'res://menu_smoke_test.gd']
    if args.headless:
        command.append('--headless')
    command += ['--', '--no-debug-overlay', '--capture-dir=' + str(capture_dir)]
    if args.default_config:
        command.append('--debug')
    if not args.default_config:
        command += ['--game-dir=' + str(game_directory())]
    options = {}
    if os.name == 'nt':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        options['startupinfo'] = startup
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120, **options)
    output = result.stdout + '\n' + result.stderr
    log_name = 'frontend_smoke_headless.log' if args.headless else 'frontend_smoke.log'
    (ROOT / 'build' / log_name).write_text(output, encoding='utf-8')
    print(output)
    marker = 'ROADWARS_RACE_MENU_SMOKE_OK' if args.races else 'ROADWARS_MENU_SMOKE_OK'
    if result.returncode or 'ERROR:' in output or 'leaked' in output.lower() or marker not in output:
        return 1
    if not args.headless:
        print('Frontend captures:', capture_dir)
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
