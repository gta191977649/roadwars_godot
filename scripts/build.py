"""Build portable core or pinned Godot extension without initializing a Git repository."""
import argparse
import hashlib
import io
import json
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path
from local_config import load_config, game_directory, resolve_track, resolve_car

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / 'scripts/dependencies.lock.json').read_text())

def fetch_cpp():
    from fetch_overlay import fetch_overlay
    fetch_overlay()
    destination = ROOT / 'platform/godot/third_party/godot-cpp'
    marker = destination / '.roadwars_commit'
    commit = LOCK['godot_cpp_commit']
    if destination.exists():
        if marker.exists() and marker.read_text().strip() == commit:
            print('Pinned godot-cpp is already present')
            return
        raise ValueError('godot-cpp directory exists without matching lock marker; preserve it and resolve manually')
    url = f'https://codeload.github.com/godotengine/godot-cpp/zip/{commit}'
    with urllib.request.urlopen(url, timeout=60) as response:
        payload = response.read()
    if hashlib.sha256(payload).hexdigest() != LOCK['godot_cpp_archive_sha256']:
        raise ValueError('godot-cpp archive checksum mismatch')
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        prefix = f'godot-cpp-{commit}/'
        for entry in archive.infolist():
            relative = Path(entry.filename.removeprefix(prefix))
            if not entry.filename.startswith(prefix) or relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Unsafe dependency archive member')
        for entry in archive.infolist():
            relative = Path(entry.filename[len(prefix):])
            if entry.is_dir():
                (destination / relative).mkdir(parents=True, exist_ok=True)
            else:
                target = destination / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(entry))
    marker.write_text(commit + '\n')
    print('Fetched godot-cpp', commit, 'archive SHA256', hashlib.sha256(payload).hexdigest())

def run(command):
    subprocess.run([str(item) for item in command], cwd=ROOT, check=True)

def positive_jobs(value):
    try:
        jobs = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError('--jobs must be a positive integer') from None
    if jobs < 1:
        raise argparse.ArgumentTypeError('--jobs must be a positive integer')
    return jobs


def positive_laps(value):
    try:
        laps = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError('--laps must be a positive integer') from None
    if not 1 <= laps <= 10:
        raise argparse.ArgumentTypeError('--laps must be an integer from 1 to 10')
    return laps


def ai_count(value):
    try:
        count = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError('--ai must be an integer from 0 to 5') from None
    if not 0 <= count <= 5:
        raise argparse.ArgumentTypeError('--ai must be an integer from 0 to 5')
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--debug', action='store_true', help='Build Debug and enable test-scene diagnostics')
    parser.add_argument('action', nargs='?', choices=['deps', 'core', 'godot', 'test', 'run', 'trace'])
    parser.add_argument('track', nargs='?', help='Original track name for trace mode, quoted when it contains spaces')
    parser.add_argument('--preset', choices=['core', 'godot'], default='core')
    parser.add_argument('--jobs', type=positive_jobs, default=4,
                        metavar='N', help='Parallel build jobs (positive integer; default: 4)')
    parser.add_argument('--original-loop', action='store_true', help='Trace recovered-loop gaps and fail at the first unimplemented stage')
    parser.add_argument('--diagnostic', action='store_true', help='Run the diagnostic triangle instead of the frontend')
    parser.add_argument('--frontend-screen', help='Preview a frontend screen by its portable id')
    parser.add_argument('--frontend-time', type=float, default=1.0, help='Frontend preview animation time')
    parser.add_argument('--frontend-variant', default='', help='Frontend preview variant')
    parser.add_argument('--race', action='store_true', help='Build Debug and launch an AI race')
    parser.add_argument('--track', dest='race_track', help='Original track name (default: Dark Forest for --race)')
    parser.add_argument('--car', help='Player car name (default: Animal)')
    parser.add_argument('--laps', type=positive_laps, help='Number of laps, 1..10 (default: 3 for --race)')
    parser.add_argument('--ai', type=ai_count, help='AI opponents, 0..5 (default: 5 for --race)')
    args = parser.parse_intermixed_args()
    if args.action is None:
        if not args.race:
            parser.error('an action is required unless --race is specified')
        args.action = 'run'
    if args.race and (args.action != 'run' or args.original_loop or args.diagnostic or args.frontend_screen):
        parser.error('--race requires run and cannot be combined with other launch modes')
    if args.track and args.race_track:
        parser.error('specify the track either positionally or with --track')
    if args.race and args.track:
        parser.error('--race uses --track rather than a positional track')
    if not args.race and (args.laps is not None or args.ai is not None):
        parser.error('--laps and --ai require --race')
    if not args.race and args.action != 'trace' and (args.car or args.race_track or args.track):
        parser.error('track and car selection require trace or --race')
    if args.action == 'deps':
        fetch_cpp()
    elif args.action in {'core', 'godot'}:
        if args.action == 'godot' and not (ROOT / 'platform/godot/third_party/godot-cpp/.roadwars_commit').exists():
            raise ValueError('Missing godot-cpp. Run python scripts/build.py deps first')
        run(['cmake', '--preset', args.action])
        run(['cmake', '--build', '--preset', args.action, '--parallel', args.jobs])
    elif args.action == 'test':
        run(['ctest', '--preset', args.preset])
    else:
        if args.action == 'trace' or args.race:
            selected_track = args.race_track or args.track or ('Dark Forest' if args.race else None)
            if not selected_track:
                raise ValueError('Usage: .\\build.ps1 --debug trace "Dark Forest"')
            track = resolve_track(selected_track)
            vehicle = resolve_car(args.car) if args.car else 'Animal'
            directory = game_directory()
            if not (ROOT / 'platform/godot/third_party/godot-cpp/.roadwars_commit').exists():
                raise ValueError('Run python scripts/build.py deps first')
            run(['cmake', '--preset', 'godot', '-DCMAKE_BUILD_TYPE=Debug'])
            run(['cmake', '--build', '--preset', 'godot', '--parallel', args.jobs])
        config = ROOT / 'config.local.json'
        data = load_config() if config.exists() else {}
        executable = data.get('godot_executable') or shutil.which('godot') or shutil.which('godot4')
        if not executable:
            raise ValueError('Set godot_executable in config.local.json or put Godot 4.7.2 on PATH')
        if (args.action == 'trace' or args.race) and sys.platform == 'win32':
            engine = Path(executable)
            console = engine.with_name(engine.stem + '_console.exe')
            if console.is_file():
                executable = str(console)
        version = subprocess.check_output([executable, '--version'], text=True).strip()
        if not version.startswith(LOCK['godot_version'] + '.'):
            raise ValueError('Expected Godot ' + LOCK['godot_version'] + ', found ' + version)
        command = [executable, '--path', ROOT / 'platform/godot/project']
        if args.race:
            command += ['--', '--race', '--race-track=' + track, '--race-vehicle=' + vehicle,
                        '--race-laps=' + str(args.laps if args.laps is not None else 3),
                        '--race-ai=' + str(args.ai if args.ai is not None else 5),
                        '--game-dir=' + str(directory)]
        elif args.action == 'trace':
            command += ['--', '--race-track=' + track, '--race-vehicle=' + vehicle, '--game-dir=' + str(directory)]
        elif args.original_loop:
            command += ['--', '--original-loop']
        elif args.diagnostic:
            command += ['--', '--diagnostic']
        else:
            command += ['--', '--game-dir=' + str(game_directory())]
            if args.frontend_screen:
                command += ['--frontend-screen=' + args.frontend_screen,
                            '--frontend-time=' + str(args.frontend_time),
                            '--frontend-variant=' + args.frontend_variant]
        if args.debug:
            if '--' not in command:
                command.append('--')
            command.append('--debug')
        run(command)

if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
