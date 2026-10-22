"""Verify real weapon/nitrous notification production, camera views and lifetime."""
import argparse
import subprocess
import sys
from local_config import ROOT, load_config, game_directory

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true')
    args = parser.parse_args()
    captures = ROOT / 'build' / ('ammunition_headless' if args.headless else 'ammunition_captures')
    captures.mkdir(parents=True, exist_ok=True)
    command = [load_config()['godot_executable'], '--path', str(ROOT / 'platform/godot/project'),
               '--script', 'res://ammunition_smoke_test.gd']
    if args.headless: command.append('--headless')
    command += ['--', '--race', '--debug', '--no-debug-overlay', '--race-track=Dark Forest', '--race-vehicle=Animal',
                '--race-ai=1', '--race-seed=1', '--game-dir=' + str(game_directory()),
                '--capture-dir=' + str(captures)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
    output = result.stdout + '\n' + result.stderr
    (captures / 'smoke.log').write_text(output, encoding='utf-8')
    print(output)
    return int(result.returncode != 0 or 'ERROR:' in output or 'leaked' in output.lower()
               or 'ROADWARS_AMMUNITION_SMOKE_OK' not in output)

if __name__ == '__main__': sys.exit(main())
