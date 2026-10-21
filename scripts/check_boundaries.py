"""Reject platform includes in the portable core."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN = re.compile(r'godot|windows|win32|d3d|ddraw|dinput|dsound|qmdx|unistd|sys/|pthread|x11/|cocoa|mach/', re.I)

def check(root=ROOT):
    errors = []
    for path in (root / 'game_src').rglob('*'):
        if path.suffix not in {'.cpp', '.hpp', '.h', '.cc'}:
            continue
        for line, value in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            match = re.match(r'\s*#\s*include\s*[<"]([^>"]+)', value)
            if match:
                local = (root / match[1]).resolve()
                if local.is_file() and local.is_relative_to((root / 'game_src').resolve()):
                    continue
            if match and (FORBIDDEN.search(match[1]) or match[1].startswith('platform/')):
                errors.append(f'{path.relative_to(root)}:{line}: platform dependency {match[1]}')
    return errors

if __name__ == '__main__':
    errors = check()
    print('\n'.join(errors) if errors else 'Portable core boundary check passed')
    sys.exit(bool(errors))
