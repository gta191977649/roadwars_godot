"""Validate sound preferences or actual Godot spatial mixer output using original assets."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
from local_config import ROOT, game_directory, load_config


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--spatial', action='store_true')
    parser.add_argument('--headless', action='store_true')
    args = parser.parse_args()
    engine = Path(load_config()['godot_executable'])
    console = engine.with_name(engine.stem + '_console.exe')
    if os.name == 'nt' and console.is_file():
        engine = console
    script = 'spatial_audio_smoke_test' if args.spatial else 'sound_options_smoke_test'
    command = [str(engine), '--path', str(ROOT / 'platform/godot/project'),
               '--script', 'res://' + script + '.gd']
    if args.headless:
        command.append('--headless')
    command += ['--', '--no-debug-overlay', '--game-dir=' + str(game_directory()),
                '--capture-dir=' + str(ROOT / 'build')]
    if args.spatial:
        command += ['--race-track=Dark Forest', '--race-vehicle=Animal']
    options = {}
    if os.name == 'nt':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        options['startupinfo'] = startup
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                            timeout=90, **options)
    output = result.stdout + '\n' + result.stderr
    (ROOT / 'build' / (script + '.log')).write_text(output, encoding='utf-8')
    print(output)
    marker = 'ROADWARS_SPATIAL_AUDIO_SMOKE_OK' if args.spatial else 'ROADWARS_SOUND_OPTIONS_SMOKE_OK'
    return int(result.returncode != 0 or marker not in output or 'ERROR:' in output or
               'leaked' in output.lower())


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
