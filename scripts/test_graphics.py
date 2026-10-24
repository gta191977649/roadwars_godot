"""Run reconstructed GPU settings smoke tests and restore all user graphics preferences."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', action='store_true')
    args = parser.parse_args()
    local = json.loads((ROOT / 'config.local.json').read_text())
    engine = local['godot_executable']
    # Godot's default user directory for this project; never delete another project directory.
    if sys.platform == 'win32':
        userdata = Path(os.environ['APPDATA']) / 'Godot/app_userdata/RoadWars Reconstruction'
    elif sys.platform == 'darwin':
        userdata = Path.home() / 'Library/Application Support/Godot/app_userdata/RoadWars Reconstruction'
    else:
        userdata = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'godot/app_userdata/RoadWars Reconstruction'
    names = ('graphics.cfg', 'graphics.last-good.cfg', 'graphics.cfg.tmp')
    previous = {name: (userdata / name).read_bytes() if (userdata / name).exists() else None for name in names}
    base = [engine, '--path', str(ROOT / 'platform/godot/project'), '--resolution', '960x540']
    def run(script, driver=None, assets=False):
        command = base + (['--rendering-driver', driver, '--rendering-method',
                           'gl_compatibility' if driver == 'opengl3_angle' else 'forward_plus'] if driver else []) + ['--script', 'res://' + script, '--', '--no-debug-overlay']
        command += ['--game-dir=' + local['game_dir']] if assets else ['--diagnostic']
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
        print(result.stdout)
        print(result.stderr, file=sys.stderr)
        if result.returncode or 'ERROR:' in result.stderr:
            raise RuntimeError('GPU graphics smoke failed: ' + script)
    try:
        for name in names:
            (userdata / name).unlink(missing_ok=True)
        run('graphics_smoke_test.gd', 'vulkan')
        run('graphics_startup_smoke_test.gd')
        if sys.platform == 'win32':
            run('graphics_quality_smoke_test.gd', 'opengl3_angle')
            run('graphics_dx11_smoke_test.gd', 'opengl3_angle')
            run('graphics_startup_smoke_test.gd')
        if args.assets:
            # Chrome fixture expects enabled starting preference.
            for name in names:
                (userdata / name).unlink(missing_ok=True)
            asset_driver = 'opengl3_angle' if sys.platform == 'win32' else 'vulkan'
            run('chrome_assets_smoke_test.gd', asset_driver, True)
            run('graphics_menu_smoke_test.gd', asset_driver, True)
    finally:
        userdata.mkdir(parents=True, exist_ok=True)
        for name, contents in previous.items():
            path = userdata / name
            if contents is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(contents)

if __name__ == '__main__':
    main()
