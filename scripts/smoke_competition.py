"""Run an original-asset AI race to a real finish in the portable core."""
import argparse
import os
import subprocess
import sys

from local_config import ROOT, game_directory, resolve_car, resolve_track


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', action='store_true', help='Run portable race acceptance (default)')
    parser.add_argument('--frame-independent', action='store_true',
                        help='Compare complete races at eight frame rates, stalls and debug reads')
    parser.add_argument('--track', default='Dark Forest')
    parser.add_argument('--car', default='Animal')
    parser.add_argument('--ai', type=int, default=5)
    parser.add_argument('--laps', type=int, default=1,
                        help='Acceptance race length; the game launcher defaults to three laps')
    args = parser.parse_args()
    if not 0 <= args.ai <= 5 or not 1 <= args.laps <= 10:
        raise ValueError('Competition acceptance requires 0..5 AI and 1..10 laps')
    track, car = resolve_track(args.track), resolve_car(args.car)
    probe = ROOT / 'build/core' / ('roadwars_race_session_test' + ('.exe' if os.name == 'nt' else ''))
    if not probe.is_file():
        raise ValueError('Build the portable core before competition acceptance')
    command = [str(probe), str(game_directory()), track, car, str(args.ai), '--laps=' + str(args.laps)]
    if args.frame_independent:
        command.append('--frame-independent')
    label = '_'.join(''.join(c if c.isalnum() else '_' for c in value.lower())
                     for value in (track, car))
    log = ROOT / 'build' / ('competition_' + label + '_' + str(args.ai) + '_' +
                          str(args.laps) + ('_timing' if args.frame_independent else '') + '.log')
    with log.open('w', encoding='utf-8') as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                text=True, timeout=600)
    output = log.read_text(encoding='utf-8', errors='replace')
    print(output)
    marker = 'RACE_FRAME_INDEPENDENT_OK' if args.frame_independent else 'ROADWARS_RACE_SESSION_OK'
    if result.returncode or marker not in output:
        return 1
    print('Competition acceptance log:', log)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
