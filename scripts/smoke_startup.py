"""Exercise asset discovery, explicit/debug precedence, retry and no persistence."""
import argparse
import subprocess
import struct
import sys
from local_config import ROOT, load_config, game_directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', action='store_true')
    args = parser.parse_args()
    config = load_config()
    game = game_directory()
    fixture = ROOT / 'build/startup-fixture/\u4e2d\u6587 assets'
    for name in ['ini/roadwars.ini', 'screens/selectgameoption/data.ini', 'fonts/16font1.bmp', 'fonts/16screentitlefont.bmp']:
        path = fixture / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'fixture')
    # A minimal original-format archive exercises packed font validation.
    index = bytearray()
    archive = bytearray()
    for name in ['Fonts/16Font1.bmp', 'Fonts/16ScreenTitleFont.bmp']:
        encoded = bytes((v + 1) & 255 for v in name.encode()) + b'\0'
        index += struct.pack('<I', len(encoded)) + encoded + struct.pack('<III', len(archive), 0, 7)
        archive += b'fixture'
    (fixture / 'data.pkh').write_bytes(index)
    (fixture / 'data.pak').write_bytes(archive)
    for name in ['cars', 'tracks', 'sfx']:
        (fixture / name).mkdir(exist_ok=True)
    project = ROOT / 'platform/godot/project'
    output_dir = ROOT / 'build/startup-captures'
    output_dir.mkdir(exist_ok=True)
    def run(name, cwd, options, headless=True, expected=game, success=True):
        command = [config['godot_executable'], '--path', str(project), '--script', 'res://startup_smoke_test.gd']
        if headless: command.append('--headless')
        command += ['--', '--no-debug-overlay', '--test-root=' + str(expected), '--test-fixture=' + str(fixture)] + options
        result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=90)
        output = result.stdout + '\n' + result.stderr
        (output_dir / (name + '.log')).write_text(output, encoding='utf-8')
        if success:
            if result.returncode or 'ERROR:' in output or 'leaked' in output.lower() or 'ROADWARS_STARTUP_SMOKE_OK' not in output:
                raise ValueError(name + ': ' + output)
        elif not result.returncode or 'Original game assets were not found' not in output:
            raise ValueError(name + ': expected explicit headless failure: ' + output)
        print('PASS', name, flush=True)
    # Godot --path changes the editor process CWD to the project. Exported CWD
    # discovery is checked separately by smoke_package.py.
    run('explicit-relative', ROOT, ['--game-dir=' + __import__('os').path.relpath(game, project)])
    run('explicit', ROOT, ['--game-dir=' + str(game)])
    run('debug-config', ROOT, ['--debug'])
    run('debug-over-cwd', fixture, ['--debug'])
    run('no-debug-config', ROOT, [], success=False)
    if args.native:
        run('retry-cancel-select', ROOT, ['--test-wait'], headless=False)
        run('no-persistence', ROOT, ['--test-wait'], headless=False)
        run('invalid-explicit', game, ['--game-dir=does-not-exist', '--test-wait'], headless=False)
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
