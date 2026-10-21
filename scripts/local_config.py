"""Single machine-local configuration entry point. No game-directory fallback."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def load_config(root=ROOT):
    path = root / 'config.local.json'
    if not path.is_file():
        raise ValueError('Create config.local.json from config.example.json and set game_dir and godot_executable')
    return json.loads(path.read_text(encoding='utf-8'))

def game_directory(root=ROOT):
    value = load_config(root).get('game_dir', '')
    if not value:
        raise ValueError('Set game_dir in config.local.json to the original Road Wars installation')
    directory = Path(value).expanduser().resolve()
    if not (directory / 'Data.pak').is_file() or not (directory / 'Data.pkh').is_file():
        raise ValueError('game_dir must contain Data.pak and Data.pkh: ' + str(directory))
    return directory

def resolve_track(name, root=ROOT):
    directory = game_directory(root) / 'Tracks'
    tracks = sorted(p.name for p in directory.iterdir() if p.is_dir() and p.name.casefold() != 'shared')
    matches = [track for track in tracks if track.casefold() == name.strip().casefold()]
    if len(matches) != 1:
        raise ValueError('Unknown or ambiguous track: ' + name + '. Available: ' + ', '.join(tracks))
    return matches[0]
