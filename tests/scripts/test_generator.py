import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from generate_classes import generate
from import_inventory import merge_inventory
from tracking import save_rows, load_rows, synchronize, validate

class GeneratorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.digest = 'a' * 64
        self.inventory = dict(binary_sha256=self.digest, function_count=1, class_count=1,
            functions=[dict(entry_point='00401000', name='Tick', namespace='Car', signature='undefined Tick(void)')],
            classes=[dict(name='class Car', rtti0_address='00601000')])
        folder = self.root / 'docs/agent/evidence'
        folder.mkdir(parents=True)
        (folder / 'roadwars_inventory.json').write_text(json.dumps(self.inventory))
        self.layout = dict(default_directory='game_src/unresolved/types', classes={})
        (folder.parent / 'class_layout.json').write_text(json.dumps(self.layout))
        rows, _ = merge_inventory(self.inventory, [], 'inventory.json')
        save_rows(rows, self.root)

    def test_generation_idempotence_stub_status_and_hand_edit(self):
        generate(self.root)
        files = list((self.root / 'game_src').rglob('*.cpp'))
        self.assertEqual(len(files), 1)
        original = files[0].read_text()
        self.assertIn('not_implemented(', original)
        self.assertIn('// RE-STUB:', original)
        before = {p: p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        generate(self.root, check=True)
        generate(self.root)
        self.assertEqual(before, {p: p.read_bytes() for p in before})
        rows = load_rows(self.root)
        function = next(r for r in rows if r['kind'] == 'function')
        self.assertTrue(function['source_path'])
        self.assertEqual(function['status'], 'not_started')
        function['status'] = 'implemented'
        self.assertTrue(any('requires source' in e for e in validate(rows, self.root)))
        files[0].write_text(original + '// Hand edit preserved\n')
        generate(self.root)
        self.assertTrue(files[0].read_text().endswith('// Hand edit preserved\n'))
        rows = load_rows(self.root)
        function = next(r for r in rows if r['kind'] == 'function')
        function['original_name'] = 'Changed'
        save_rows(rows, self.root)
        with self.assertRaisesRegex(ValueError, 'manual merge'):
            generate(self.root)
        self.assertTrue(files[0].read_text().endswith('// Hand edit preserved\n'))

    def test_path_requires_evidence_and_stays_in_workspace(self):
        key = self.digest + ':class:00601000'
        path = self.root / 'docs/agent/class_layout.json'
        self.layout['classes'][key] = dict(directory='game_src/cars')
        path.write_text(json.dumps(self.layout))
        with self.assertRaisesRegex(ValueError, 'evidence'): generate(self.root)
        self.layout['classes'][key] = dict(directory='game_src/../../outside', evidence='evidence')
        path.write_text(json.dumps(self.layout))
        with self.assertRaisesRegex(ValueError, 'safe'): generate(self.root)
        self.layout['classes'][key] = dict(directory='game_src/confirmed_module', evidence='Ghidra source path')
        path.write_text(json.dumps(self.layout))
        generate(self.root)
        self.assertEqual(len(list((self.root / 'game_src/confirmed_module').glob('*.cpp'))), 1)

if __name__ == '__main__': unittest.main()
