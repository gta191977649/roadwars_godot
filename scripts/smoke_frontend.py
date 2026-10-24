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
    parser.add_argument('--driver', choices=('vulkan', 'd3d12', 'metal', 'opengl3_angle'))
    parser.add_argument('--races', action='store_true', help='Check live Single/Quick menu launches, Loading and BGM')
    parser.add_argument('--garage-ui', action='store_true', help='Capture and check actual Garage stores, rows and confirmations at two resolutions')
    parser.add_argument('--attract', action='store_true', help='Check Title idle demo, black transitions and return')
    parser.add_argument('--replay', action='store_true', help='Check real Replay controls and live-state restoration with a diagnostic recorder')
    parser.add_argument('--default-config', action='store_true', help='Exercise the no-game-dir host config path')
    args = parser.parse_args()
    config = json.loads((ROOT / 'config.local.json').read_text())
    capture_dir = ROOT / 'build' / 'frontend_captures'
    capture_dir.mkdir(parents=True, exist_ok=True)
    script = 'garage_ui_smoke_test' if args.garage_ui else 'replay_smoke_test' if args.replay else 'title_attract_smoke_test' if args.attract else 'race_menu_smoke_test' if args.races else 'menu_smoke_test'
    command = [config['godot_executable'], '--path', str(ROOT / 'platform/godot/project'),
               '--script', 'res://' + script + '.gd']
    if args.driver:
        command += ['--rendering-driver', args.driver, '--rendering-method',
                    'gl_compatibility' if args.driver == 'opengl3_angle' else 'forward_plus']
    if args.headless:
        command.append('--headless')
    command += ['--', '--no-debug-overlay', '--capture-dir=' + str(capture_dir)]
    if args.replay:
        command += ['--race', '--race-ai=1', '--race-replay-seconds=30',
                    '--capture=' + str(capture_dir / 'replay.png')]
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
    if args.attract:
        log_name = 'title_attract_smoke' + ('_headless' if args.headless else '') + '.log'
    if args.replay:
        log_name = 'replay_smoke' + ('_headless' if args.headless else '') + '.log'
    if args.garage_ui:
        log_name = 'garage_ui_smoke' + ('_headless' if args.headless else '') + '.log'
    (ROOT / 'build' / log_name).write_text(output, encoding='utf-8')
    print(output)
    marker = 'ROADWARS_GARAGE_UI_SMOKE_OK' if args.garage_ui else 'ROADWARS_REPLAY_SMOKE_OK' if args.replay else 'ROADWARS_TITLE_ATTRACT_SMOKE_OK' if args.attract else 'ROADWARS_RACE_MENU_SMOKE_OK' if args.races else 'ROADWARS_MENU_SMOKE_OK'
    if (result.returncode or 'ERROR:' in output or 'leaked' in output.lower() or marker not in output
            or (args.driver == 'd3d12' and not args.headless and 'D3D12 ' not in output)
            or (args.driver == 'opengl3_angle' and not args.headless and 'Direct3D11' not in output)):
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
