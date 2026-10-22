"""Verify cockpit camera controls, native aspect geometry and race lifecycle."""
import argparse
import subprocess
import sys
from local_config import ROOT, load_config, game_directory, resolve_car


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--car', default='Animal')
    parser.add_argument('--visual-only', action='store_true')
    args = parser.parse_args()
    car = resolve_car(args.car)
    suffix = '' if args.visual_only else ('_headless_test' if args.headless else '_test')
    captures = ROOT / 'build' / ('cockpit_' + car.lower().replace(' ', '_') + suffix)
    captures.mkdir(parents=True, exist_ok=True)
    command = [load_config()['godot_executable'], '--path', str(ROOT / 'platform/godot/project'),
               '--script', 'res://cockpit_smoke_test.gd']
    if args.headless:
        command.append('--headless')
    command += ['--', '--race', '--race-track=Dark Forest', '--race-vehicle=' + car,
                '--race-ai=5', '--game-dir=' + str(game_directory()), '--capture-dir=' + str(captures)]
    if args.visual_only:
        command += ['--cockpit-visual-only', '--no-debug-overlay']
    else:
        command.append('--debug')
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
    output = result.stdout + '\n' + result.stderr
    (captures / 'smoke.log').write_text(output, encoding='utf-8')
    print(output)
    return int(result.returncode != 0 or 'ERROR:' in output or 'leaked' in output.lower()
               or 'ROADWARS_COCKPIT_SMOKE_OK' not in output)


if __name__ == '__main__':
    sys.exit(main())
