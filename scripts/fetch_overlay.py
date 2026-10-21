"""Fetch hash-pinned ImGui and imgui-console, including its bundled csys headers."""
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def fetch_overlay():
    lock = json.loads((ROOT / 'scripts/dependencies.lock.json').read_text())
    for name in ('imgui', 'imgui-console'):
        dependency = lock[name]
        destination = ROOT / 'platform/godot/third_party' / name
        marker = destination / '.roadwars_commit'
        if destination.exists():
            if marker.is_file() and marker.read_text().strip() == dependency['commit']:
                continue
            raise ValueError(f'{name} exists without matching lock marker; preserve it and resolve manually')
        url = f"https://codeload.github.com/{dependency['repository']}/zip/{dependency['commit']}"
        with urllib.request.urlopen(url, timeout=60) as response:
            payload = response.read()
        if hashlib.sha256(payload).hexdigest() != dependency['sha256']:
            raise ValueError(f'{name} archive checksum mismatch')
        with zipfile.ZipFile(io.BytesIO(payload)) as archive:
            prefix = archive.namelist()[0]
            for entry in archive.infolist():
                relative = PurePosixPath(entry.filename.removeprefix(prefix))
                if not entry.filename.startswith(prefix) or relative.is_absolute() or '..' in relative.parts:
                    raise ValueError(f'Unsafe {name} archive member')
            for entry in archive.infolist():
                if entry.is_dir():
                    continue
                relative = PurePosixPath(entry.filename.removeprefix(prefix))
                target = destination.joinpath(*relative.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(entry))
        marker.write_text(dependency['commit'] + '\n')


if __name__ == '__main__':
    fetch_overlay()
