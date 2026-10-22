"""Capture bounded native frame attribution and resource/pipeline creation during a live race."""
import argparse
import csv
import json
from pathlib import Path
import re
import subprocess

from local_config import ROOT, game_directory, load_config


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=25)
    parser.add_argument('--label', default='capture')
    parser.add_argument('--without-debug', action='store_true')
    args = parser.parse_args()
    if not 0 < args.seconds <= 120 or not re.fullmatch(r'[a-zA-Z0-9_-]+', args.label):
        parser.error('Use 0 < seconds <= 120 and a simple alphanumeric label')
    executable = Path(load_config()['godot_executable'])
    console = executable.with_name(executable.stem + '_console.exe')
    if console.is_file():
        executable = console
    command = [str(executable), '--path', str(ROOT / 'platform/godot/project'),
               '--script', 'res://frame_profile_capture.gd', '--', '--frame-trace', '--race',
               '--race-track=Dark Forest', '--race-vehicle=Animal', '--race-ai=5', '--race-laps=3',
               '--race-seed=1', '--game-dir=' + str(game_directory()), '--debug',
               '--profile-label=' + args.label, '--profile-seconds=' + str(args.seconds)]
    if args.without_debug:
        command.append('--no-debug-overlay')
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                            timeout=args.seconds + 90)
    log = result.stdout + '\n' + result.stderr
    (ROOT / ('build/frame_profile_' + args.label + '.log')).write_text(log, encoding='utf-8')
    print(log)
    if result.returncode or 'ROADWARS_FRAME_PROFILE_OK' not in log or 'ERROR:' in log or 'leaked' in log:
        return 1
    payload = json.loads((ROOT / ('build/frame_profile_' + args.label + '.json')).read_text())
    rows = payload['frames']
    if not rows:
        raise ValueError('No native frame attribution was captured')
    with (ROOT / ('build/frame_profile_' + args.label + '.csv')).open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
