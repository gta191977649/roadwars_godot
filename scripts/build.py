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

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / 'scripts/dependencies.lock.json').read_text())

def fetch_cpp():
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

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['deps', 'core', 'godot', 'test', 'run'])
    parser.add_argument('--preset', choices=['core', 'godot'], default='core')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--original-loop', action='store_true', help='Trace recovered-loop gaps and fail at the first unimplemented stage')
    args = parser.parse_args()
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
        config = ROOT / 'config.local.json'
        data = json.loads(config.read_text()) if config.exists() else {}
        executable = data.get('godot_executable') or shutil.which('godot') or shutil.which('godot4')
        if not executable:
            raise ValueError('Set godot_executable in config.local.json or put Godot 4.7.2 on PATH')
        version = subprocess.check_output([executable, '--version'], text=True).strip()
        if not version.startswith(LOCK['godot_version'] + '.'):
            raise ValueError('Expected Godot ' + LOCK['godot_version'] + ', found ' + version)
        command = [executable, '--path', ROOT / 'platform/godot/project']
        if args.original_loop:
            command += ['--', '--original-loop']
        run(command)

if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
