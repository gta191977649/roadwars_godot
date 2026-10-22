"""Exercise real ImGui input, DPI, logging and overlay teardown in Godot."""
import argparse
from pathlib import Path
import subprocess
from local_config import ROOT, load_config, game_directory, resolve_track


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--track', help='Use original assets instead of the diagnostic triangle')
    args = parser.parse_args()
    executable = Path(load_config()['godot_executable'])
    console = executable.with_name(executable.stem + '_console.exe')
    if console.is_file():
        executable = console
    command = [str(executable), '--path', str(ROOT / 'platform/godot/project'),
               '--script', 'res://debug_overlay_smoke_test.gd']
    if args.headless:
        command.append('--headless')
    if args.track:
        command += ['--', '--race-track=' + resolve_track(args.track),
                    '--game-dir=' + str(game_directory())]
    else:
        command += ['--', '--diagnostic']
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
    print(result.stdout, result.stderr)
    # The fixture deliberately generates one warning/error and a stderr message.
    if result.returncode or 'ROADWARS_DEBUG_OVERLAY_SMOKE_OK' not in result.stdout:
        return 1
    if 'leaked' in result.stderr.lower() or 'SCRIPT ERROR:' in result.stderr:
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
