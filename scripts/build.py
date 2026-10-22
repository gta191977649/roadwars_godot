"""Build incrementally; launch Godot only when --run is explicitly supplied."""
import argparse
import contextlib
import errno
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import time
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


def build_inputs(preset):
    paths = ['CMakePresets.json', 'scripts/dependencies.lock.json']
    if preset == 'godot':
        paths.append('platform/godot/build_profile.json')
    return [ROOT / path for path in paths if (ROOT / path).is_file()]


def build_directory(preset):
    directory = (ROOT / 'build' / preset).resolve()
    if not directory.is_relative_to(ROOT.resolve()):
        raise ValueError('Build directory resolves outside the workspace')
    return directory


@contextlib.contextmanager
def build_lock(preset):
    """Serialize configure, depfile ingestion and build for one Ninja database."""
    directory = build_directory(preset)
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / '.roadwars_build.lock').open('a+b') as lock:
        lock.seek(0, os.SEEK_END)
        if not lock.tell():
            lock.write(b'0')
            lock.flush()
        if sys.platform == 'win32':
            import msvcrt
            def acquire():
                lock.seek(0)
                msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
            def release():
                lock.seek(0)
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl
            def acquire():
                fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            def release():
                fcntl.flock(lock.fileno(), fcntl.LOCK_UN)
        waiting = False
        deadline = time.monotonic() + 600
        while True:
            try:
                acquire()
                break
            except OSError as error:
                if error.errno not in {errno.EACCES, errno.EAGAIN, errno.EDEADLK}:
                    raise
                if time.monotonic() >= deadline:
                    raise ValueError(f'Timed out waiting for the {preset} build lock') from error
                if not waiting:
                    print(f'Waiting for the active {preset} build to finish...', flush=True)
                    waiting = True
                time.sleep(.2)
        try:
            yield
        finally:
            release()


def empty_dependency_objects(preset):
    directory = build_directory(preset)
    if not (directory / 'build.ninja').is_file():
        return []
    output = subprocess.check_output(['ninja', '-C', str(directory), '-t', 'deps'], text=True)
    result = []
    for line in output.splitlines():
        match = re.match(r'^(.+): #deps 0,', line)
        if not match:
            continue
        target = (directory / match.group(1)).resolve()
        # GCC -MD always records at least the source. Zero deps is a broken
        # ingestion/cache entry, not proof an object is safely up to date.
        if not target.is_relative_to(directory) or target.suffix not in {'.obj', '.o'}:
            raise ValueError('Unsafe object path in Ninja dependency cache: ' + match.group(1))
        result.append(target)
    return result


def configure_build(preset):
    """Keep configure-only generators from invalidating unchanged godot-cpp bindings."""
    directory = build_directory(preset)
    cache = directory / 'CMakeCache.txt'
    manifest = directory / 'build.ninja'
    stamp = directory / '.roadwars_build_inputs.json'
    inputs = build_inputs(preset)
    signatures = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in inputs}
    valid = cache.is_file() and manifest.is_file()
    if valid:
        values = {}
        for line in cache.read_text(encoding='utf-8').splitlines():
            if '=' in line and ':' in line.split('=', 1)[0]:
                key, value = line.split('=', 1)
                values[key.split(':', 1)[0]] = value
        home = values.get('CMAKE_HOME_DIRECTORY', '')
        valid = (Path(home).resolve() == ROOT.resolve() and
                 values.get('CMAKE_GENERATOR') == 'Ninja' and
                 values.get('CMAKE_BUILD_TYPE') == 'Debug' and
                 values.get('ROADWARS_BUILD_GODOT') == ('ON' if preset == 'godot' else 'OFF'))
    if valid and stamp.is_file():
        try:
            valid = json.loads(stamp.read_text()) == signatures
        except (ValueError, OSError):
            valid = False
    elif valid:
        # Adopt an existing correctly configured checkout without needlessly
        # rewriting the generated API on the first use of this script version.
        valid = all(path.stat().st_mtime_ns <= manifest.stat().st_mtime_ns for path in inputs)
    if not valid:
        print(f'Configuring {preset} (new cache or changed configuration inputs)', flush=True)
        run(['cmake', '--preset', preset])
    else:
        print(f'Using cached {preset} configuration; incremental Ninja build', flush=True)
    directory.mkdir(parents=True, exist_ok=True)
    stamp.write_text(json.dumps(signatures, sort_keys=True) + '\n')


def build_target(preset, jobs, clean=False):
    if preset == 'godot' and not (ROOT / 'platform/godot/third_party/godot-cpp/.roadwars_commit').is_file():
        raise ValueError('Missing godot-cpp. Run python scripts/build.py deps first')
    with build_lock(preset):
        configure_build(preset)
        broken = empty_dependency_objects(preset)
        for target in broken:
            if target.is_file():
                print('Rebuilding object with an empty Ninja dependency record: ' +
                      str(target.relative_to(build_directory(preset))), flush=True)
                target.unlink()
        command = ['cmake', '--build', '--preset', preset, '--parallel', jobs]
        if clean:
            command.append('--clean-first')
        run(command)
        remaining = [target for target in empty_dependency_objects(preset) if target.is_file()]
        if remaining:
            raise ValueError('Build has incomplete header dependencies; stop concurrent direct Ninja/CMake '
                             'builds and rerun: ' + ', '.join(str(path) for path in remaining))

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
    parser.add_argument('--debug', action='store_true', help='Enable diagnostics when launched (build presets already use Debug)')
    parser.add_argument('action', nargs='?', choices=['deps', 'core', 'godot', 'test', 'run', 'trace'])
    parser.add_argument('track', nargs='?', help='Original track name for trace mode, quoted when it contains spaces')
    parser.add_argument('--preset', choices=['core', 'godot'], help='Build preset (default: godot; test default: core)')
    parser.add_argument('--run', action='store_true', help='Launch Godot after the incremental build')
    parser.add_argument('--clean', action='store_true', help='Clean build targets before rebuilding; retain configuration and dependencies')
    parser.add_argument('--jobs', type=positive_jobs, default=4,
                        metavar='N', help='Parallel build jobs (positive integer; default: 4)')
    parser.add_argument('--original-loop', action='store_true', help='Trace recovered-loop gaps and fail at the first unimplemented stage')
    parser.add_argument('--diagnostic', action='store_true', help='Run the diagnostic triangle instead of the frontend')
    parser.add_argument('--frontend-screen', help='Preview a frontend screen by its portable id')
    parser.add_argument('--frontend-time', type=float, default=1.0, help='Frontend preview animation time')
    parser.add_argument('--frontend-variant', default='', help='Frontend preview variant')
    parser.add_argument('--race', action='store_true', help='Select AI race mode for --run; build only unless --run is supplied')
    parser.add_argument('--track', dest='race_track', help='Original track name (default: Dark Forest for --race)')
    parser.add_argument('--car', help='Player car name (default: Animal)')
    parser.add_argument('--laps', type=positive_laps, help='Number of laps, 1..10 (default: 3 for --race)')
    parser.add_argument('--ai', type=ai_count, help='AI opponents, 0..5 (default: 5 for --race)')
    parser.add_argument('--seed', type=int, help='Diagnostic CRT seed, 0..4294967295 (default: 1)')
    args = parser.parse_intermixed_args()
    if args.action is None:
        args.action = args.preset or 'godot'
    if args.run and args.action in {'deps', 'core', 'test'}:
        parser.error('--run requires the godot, run or trace action')
    if args.clean and args.action in {'deps', 'test'}:
        parser.error('--clean requires a build action')
    if args.race and (args.action not in {'godot', 'run'} or args.original_loop or args.diagnostic or args.frontend_screen):
        parser.error('--race requires a Godot build and cannot be combined with other launch modes')
    if args.track and args.race_track:
        parser.error('specify the track either positionally or with --track')
    if args.race and args.track:
        parser.error('--race uses --track rather than a positional track')
    if args.seed is not None and not 0 <= args.seed <= 0xffffffff:
        parser.error('--seed must be in 0..4294967295')
    if not args.race and (args.laps is not None or args.ai is not None or args.seed is not None):
        parser.error('--laps, --ai and --seed require --race')
    if not args.race and args.action != 'trace' and (args.car or args.race_track or args.track):
        parser.error('track and car selection require trace or --race')
    if args.action == 'trace' and not (args.race_track or args.track):
        parser.error('trace requires a track name, quoted when it contains spaces')
    if args.action == 'deps':
        fetch_cpp()
    elif args.action == 'test':
        run(['ctest', '--preset', args.preset or 'core'])
    else:
        target = 'core' if args.action == 'core' else 'godot'
        build_target(target, args.jobs, args.clean)
        if not args.run:
            print('Build complete. Add --run to launch Godot.', flush=True)
            return
        if args.action == 'trace' or args.race:
            selected_track = args.race_track or args.track or ('Dark Forest' if args.race else None)
            if not selected_track:
                raise ValueError('Usage: .\\build.ps1 --debug trace "Dark Forest"')
            track = resolve_track(selected_track)
            vehicle = resolve_car(args.car) if args.car else 'Animal'
            directory = game_directory()
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
                        '--race-seed=' + str(args.seed if args.seed is not None else 1),
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
