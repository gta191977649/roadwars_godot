"""Check a complete original-resource AI race, debug UI and host lifetime."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
from local_config import ROOT, load_config, game_directory, resolve_track, resolve_car


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--driver', choices=('vulkan', 'd3d12', 'metal', 'opengl3_angle'))
    parser.add_argument('--without-debug', action='store_true', help='Check startup and teardown with no debug UI')
    parser.add_argument('--track', default='Dark Forest')
    parser.add_argument('--car', default='Animal')
    parser.add_argument('--laps', type=int, choices=range(1, 11), default=1)
    parser.add_argument('--ai', type=int, choices=range(6), default=5)
    parser.add_argument('--seed', type=int, help='Use an explicit diagnostic race seed for reproducible acceptance')
    args = parser.parse_args()
    if args.seed is not None and not 0 <= args.seed <= 0xffffffff:
        raise ValueError('Diagnostic race seed must fit an unsigned 32-bit integer')
    executable = Path(load_config()['godot_executable'])
    if os.name == 'nt':
        console = executable.with_name(executable.stem + '_console.exe')
        if console.is_file():
            executable = console
    capture_dir = ROOT / 'build/competition_captures'
    capture_dir.mkdir(parents=True, exist_ok=True)
    script = 'competition_no_debug_test.gd' if args.without_debug else 'competition_smoke_test.gd'
    command = [str(executable), '--path', str(ROOT / 'platform/godot/project'),
               '--script', 'res://' + script]
    if args.driver:
        command += ['--rendering-driver', args.driver, '--rendering-method',
                    'gl_compatibility' if args.driver == 'opengl3_angle' else 'forward_plus']
    if args.headless:
        command.append('--headless')
    command += ['--', '--race', '--race-track=' + resolve_track(args.track),
                '--race-vehicle=' + resolve_car(args.car), '--race-laps=' + str(args.laps),
                '--race-ai=' + str(args.ai), '--game-dir=' + str(game_directory()),
                '--capture-dir=' + str(capture_dir)]
    if args.seed is not None:
        command.append('--race-seed=' + str(args.seed))
    if not args.without_debug:
        command.append('--debug')
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=240)
    output = result.stdout + '\n' + result.stderr
    log_name = 'competition_smoke_headless.log' if args.headless else 'competition_smoke.log'
    if args.without_debug:
        log_name = 'competition_without_debug.log'
    (ROOT / 'build' / log_name).write_text(output, encoding='utf-8')
    print(output)
    marker = 'ROADWARS_COMPETITION_NO_DEBUG_OK' if args.without_debug else 'ROADWARS_COMPETITION_SMOKE_OK'
    if (result.returncode or 'ERROR:' in output or 'leaked' in output.lower() or marker not in output
            or (args.driver == 'd3d12' and not args.headless and 'D3D12 ' not in output)
            or (args.driver == 'opengl3_angle' and not args.headless and 'Direct3D11' not in output)):
        return 1
    if not args.headless and not args.without_debug:
        for name in ('countdown', 'race_debug', 'race_debug_wide', 'race_running', 'race_results'):
            if not (capture_dir / (name + '.png')).is_file():
                raise ValueError('Missing competition capture: ' + name)
        print('Competition captures:', capture_dir)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
