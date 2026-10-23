"""Validate chrome shader pixels and resource lifetime on the real Forward+ renderer."""
import os
import argparse
from pathlib import Path
import subprocess
import sys

from local_config import load_config, game_directory

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', action='store_true',
                        help='Verify actual garage chrome, graphics toggle and lifecycle')
    args = parser.parse_args()
    executable = Path(load_config(ROOT)['godot_executable'])
    if os.name == 'nt' and not executable.stem.endswith('_console'):
        console = executable.with_name(executable.stem + '_console.exe')
        if console.is_file():
            executable = console
    project = ROOT / 'platform/godot/project'
    build = ROOT / 'build'
    build.mkdir(exist_ok=True)
    log_path = build / ('chrome_assets_smoke.log' if args.assets else 'chrome_smoke.log')
    capture = build / ('chrome_garage_on.png' if args.assets else 'chrome_fixture.png')
    log_path.unlink(missing_ok=True)
    capture.unlink(missing_ok=True)
    options = {'cwd': ROOT, 'capture_output': True, 'text': True, 'timeout': 60}
    if os.name == 'nt':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        options['startupinfo'] = startup
    version = subprocess.check_output([str(executable), '--version'], text=True).strip()
    if not version.startswith('4.7.2.'):
        raise ValueError('Godot 4.7.2 required, found ' + version)
    imported = subprocess.run(
        [str(executable), '--headless', '--path', str(project), '--editor', '--import', '--quit'],
        **options)
    print(imported.stdout, imported.stderr)
    import_output = imported.stdout + '\n' + imported.stderr
    if imported.returncode or 'ERROR:' in import_output or 'leaked' in import_output.lower():
        return 1
    script = 'chrome_assets_smoke_test.gd' if args.assets else 'chrome_smoke_test.gd'
    profile = ['--game-dir=' + str(game_directory())] if args.assets else ['--diagnostic']
    result = subprocess.run(
        [str(executable), '--log-file', str(log_path), '--path', str(project),
         '--script', 'res://' + script, '--', '--no-debug-overlay'] + profile,
        **options)
    log = log_path.read_text(encoding='utf-8', errors='replace') if log_path.is_file() else ''
    output = result.stdout + '\n' + result.stderr + '\n' + log
    print(output)
    if not any('Vulkan ' in line and ' - Forward+ - ' in line for line in output.splitlines()):
        print('Expected the default Vulkan Forward+ renderer', file=sys.stderr)
        return 1
    marker = 'ROADWARS_CHROME_ASSETS_OK' if args.assets else 'ROADWARS_CHROME_SMOKE_OK'
    if (result.returncode or 'ERROR:' in output or 'leaked' in output.lower()
            or marker not in output or not capture.is_file()):
        return 1
    print('Verified chrome render capture:', capture)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (KeyError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
