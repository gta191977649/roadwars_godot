"""Capture bounded native frame attribution and resource/pipeline creation during a live race."""
import argparse
import csv
import json
from pathlib import Path
import re
import subprocess

from local_config import ROOT, game_directory, load_config, resolve_car, resolve_track


def summarize_frames(rows, target_ms, capture_start_us=None):
    # Intervals end at the current callback; its predecessor contains the work
    # responsible for that interval. Exclude startup by elapsed time, not FPS.
    start = (rows[0]['start_us'] if capture_start_us is None else capture_start_us) + 2_000_000
    pairs = [(rows[i - 1], row) for i, row in enumerate(rows)
             if i and rows[i - 1]['start_us'] >= start]
    intervals = sorted(row['interval_us'] / 1000 for _, row in pairs)
    if not intervals:
        return {}
    threshold_ms = target_ms * 1.5 if target_ms > 0 else 25.0
    worst = sorted(pairs, key=lambda pair: pair[1]['interval_us'], reverse=True)[:10]
    return {
        'interval_count': len(intervals),
        'mean_ms': sum(intervals) / len(intervals),
        'p99_ms': intervals[max(0, (99 * len(intervals) + 99) // 100 - 1)],
        'max_ms': intervals[-1],
        'threshold_ms': threshold_ms,
        'late_intervals': sum(value > threshold_ms for value in intervals),
        'file_reads': sum(work['file_reads'] for work, _ in pairs),
        'textures': sum(work['textures'] for work, _ in pairs),
        'materials': sum(work['materials'] for work, _ in pairs),
        'worst_intervals': [dict(interval_ms=row['interval_us'] / 1000, work=work)
                            for work, row in worst],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=25)
    parser.add_argument('--label', default='capture')
    parser.add_argument('--without-debug', action='store_true')
    parser.add_argument('--track', default='Dark Forest')
    parser.add_argument('--vehicle', default='Animal')
    parser.add_argument('--seed', type=int, default=1)
    parser.add_argument('--laps', type=int, choices=range(1, 11), default=3)
    args = parser.parse_args()
    if not 0 < args.seconds <= 600 or not re.fullmatch(r'[a-zA-Z0-9_-]+', args.label):
        parser.error('Use 0 < seconds <= 600 and a simple alphanumeric label')
    if not 0 <= args.seed <= 0xffffffff:
        parser.error('Seed must fit an unsigned 32-bit integer')
    executable = Path(load_config()['godot_executable'])
    console = executable.with_name(executable.stem + '_console.exe')
    if console.is_file():
        executable = console
    command = [str(executable), '--path', str(ROOT / 'platform/godot/project'),
               '--script', 'res://frame_profile_capture.gd', '--', '--frame-trace', '--race',
               '--race-track=' + resolve_track(args.track),
               '--race-vehicle=' + resolve_car(args.vehicle), '--race-ai=5',
               '--race-laps=' + str(args.laps),
               '--race-seed=' + str(args.seed), '--game-dir=' + str(game_directory()), '--debug',
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
    summary = summarize_frames(rows, payload['timing'].get('target_ms', 0),
                               payload.get('capture_start_us'))
    (ROOT / ('build/frame_profile_' + args.label + '_summary.json')).write_text(
        json.dumps(summary, indent=2), encoding='utf-8')
    print('Retained trace:', json.dumps({key: value for key, value in summary.items()
                                       if key != 'worst_intervals'}))
    print('Race:', json.dumps({key: payload.get('race', {}).get(key)
                              for key in ('time', 'player_laps', 'terminal', 'paused')}))
    with (ROOT / ('build/frame_profile_' + args.label + '.csv')).open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
