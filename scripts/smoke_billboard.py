"""Render asymmetric billboard geometry from 24 camera views using the actual GPU shader."""
from pathlib import Path
import subprocess
from local_config import ROOT, load_config


def main():
    executable = Path(load_config()['godot_executable'])
    console = executable.with_name(executable.stem + '_console.exe')
    if console.is_file():
        executable = console
    command = [str(executable), '--path', str(ROOT / 'platform/godot/project'),
               '--script', 'res://billboard_smoke_test.gd', '--', '--diagnostic', '--no-debug-overlay']
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
    print(result.stdout, result.stderr)
    if result.returncode or 'ROADWARS_BILLBOARD_SMOKE_OK' not in result.stdout:
        return 1
    if 'ERROR:' in result.stderr or 'leaked' in result.stderr.lower():
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
