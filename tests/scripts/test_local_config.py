"""Machine configuration and quoted track names have one resolution path."""
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from local_config import game_directory, resolve_track


class LocalConfigurationTests(unittest.TestCase):
    def test_requires_explicit_game_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaises(ValueError):
                game_directory(root)
            (root / 'config.local.json').write_text('{}')
            with self.assertRaises(ValueError):
                game_directory(root)

    def test_track_name_is_not_trace_level(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / 'original'
            game.mkdir()
            for name in ('Data.pak', 'Data.pkh'):
                (game / name).touch()
            for name in ('Dark Forest', 'Shared'):
                (game / 'Tracks' / name).mkdir(parents=True)
            (root / 'config.local.json').write_text(json.dumps({'game_dir': str(game)}))
            self.assertEqual(game_directory(root), game.resolve())
            self.assertEqual(resolve_track('dark forest', root), 'Dark Forest')
            for name in ('Shared', 'n', '../Dark Forest'):
                with self.assertRaises(ValueError):
                    resolve_track(name, root)
