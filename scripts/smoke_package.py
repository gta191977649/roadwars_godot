"""Run the exported Windows EXE without the repository or developer configuration."""
import argparse
from pathlib import Path
import os
import json
import shutil
import subprocess
import sys
from local_config import ROOT, game_directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, default=ROOT / 'build/bin/windows')
    args = parser.parse_args()
    package = args.package.resolve()
    if sorted(p.name for p in package.iterdir()) != ['RoadWars-RE.exe', 'roadwars_godot.dll']:
        raise ValueError('Expected exactly the embedded-pack EXE and extension DLL')
    base = ROOT / 'build/package-smoke'
    assets = base / 'assets'
    assets.mkdir(parents=True, exist_ok=True)
    # Private read-only test copies: hard links avoid copying the original archives.
    def copy_asset(source, destination):
        if Path(destination).exists(): return destination
        try: os.link(source, destination)
        except OSError: shutil.copy2(source, destination)
        return destination
    shutil.copytree(game_directory(), assets, copy_function=copy_asset, dirs_exist_ok=True)
    def run(name, binary, cwd, arguments, expected=None):
        log = base / (name + '.log')
        command = [str(binary), '--headless', '--quit-after', '12', '--log-file', str(log)] + arguments
        result = subprocess.run(command, cwd=cwd, capture_output=True, timeout=60)
        output = log.read_text(encoding='utf-8', errors='replace') if log.exists() else ''
        if expected is None:
            if not result.returncode or 'Original game assets were not found' not in output:
                raise ValueError(name + ': expected a headless missing-assets failure: ' + output)
        elif result.returncode or 'ERROR:' in output or 'leaked' in output.lower() or ('ROADWARS_ASSETS_READY: ' + str(expected).replace('\\', '/')) not in output:
            raise ValueError(name + ': ' + output)
        print('PASS', name, flush=True)
    for relative in ['', 'bin', 'Cars/Animal/portable-bin']:
        destination = assets / relative
        destination.mkdir(parents=True, exist_ok=True)
        for name in ['RoadWars-RE.exe', 'roadwars_godot.dll']:
            shutil.copy2(package / name, destination / name)
        run('ancestor-' + (relative.replace('/', '-') or 'root'), destination / 'RoadWars-RE.exe', base, [], assets)
    isolated = base / 'isolated'
    isolated.mkdir(exist_ok=True)
    for name in ['RoadWars-RE.exe', 'roadwars_godot.dll']:
        shutil.copy2(package / name, isolated / name)
    # The shipped package has no config. A private fixture verifies that only
    # explicit --debug may read an executable-adjacent developer configuration.
    (isolated / 'config.local.json').write_text(json.dumps({'game_dir': str(assets)}), encoding='utf-8')
    run('isolated-no-config', isolated / 'RoadWars-RE.exe', base, [])
    run('direct-debug', isolated / 'RoadWars-RE.exe', base, ['--debug'], assets)
    run('separated-debug', isolated / 'RoadWars-RE.exe', base, ['--', '--debug'], assets)
    run('isolated-explicit', isolated / 'RoadWars-RE.exe', base, ['--', '--game-dir=' + str(assets)], assets)
    run('isolated-race', isolated / 'RoadWars-RE.exe', base, ['--', '--game-dir=' + str(assets), '--race', '--race-ai=1', '--race-track=Dark Forest', '--race-vehicle=Animal'], assets)
    print('ROADWARS_PACKAGE_SMOKE_OK')
    return 0

if __name__ == '__main__':
    try: sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
