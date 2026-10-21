import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from import_inventory import merge_inventory
from tracking import aggregate_classes, load_rows, render, save_rows, synchronize, validate
from check_boundaries import check

DIGEST = 'a' * 64

def inventory():
    return dict(binary_sha256=DIGEST, function_count=2, class_count=1,
                functions=[dict(entry_point='00401000', name='a', namespace='Car'), dict(entry_point='00402000', name='b', namespace='Global')],
                classes=[dict(rtti0_address='00601000', name='class Car')])

class TrackingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'game_src/unresolved').mkdir(parents=True)
        self.rows, _ = merge_inventory(inventory(), [], 'evidence.json')
        self.functions = [r for r in self.rows if r['kind'] == 'function']

    def markers(self):
        (self.root / 'game_src/unresolved/sample.cpp').write_text('\n'.join('// RE: ' + row['id'] for row in self.functions) + '\nvoid merged() {}\n')

    def test_multiple_mappings_do_not_advance_status(self):
        self.markers()
        synchronize(self.rows, self.root)
        self.assertTrue(all(r['source_path'] for r in self.functions))
        self.assertTrue(all(r['status'] == 'not_started' for r in self.functions))
        self.assertEqual(validate(self.rows, self.root), [])

    def test_unknown_class_and_marker(self):
        self.functions[0]['class_id'] = 'missing'
        self.assertTrue(any('invalid class' in e for e in validate(self.rows, self.root)))
        (self.root / 'game_src/unresolved/sample.cpp').write_text('// RE: nonexistent\n')
        self.assertTrue(any('source marker' in e for e in validate(self.rows, self.root)))

    def test_validation_and_aggregate(self):
        row = self.functions[0]
        row.update(scope='required', status='implemented')
        aggregate_classes(self.rows)
        self.assertTrue(any('requires source' in e for e in validate(self.rows, self.root)))
        self.markers()
        synchronize(self.rows, self.root)
        self.assertEqual(validate(self.rows, self.root), [])
        row.update(status='verified', evidence='')
        aggregate_classes(self.rows)
        self.assertTrue(any('requires evidence' in e for e in validate(self.rows, self.root)))
        row['status'] = 'invented'
        self.assertTrue(any('invalid status' in e for e in validate(self.rows, self.root)))

    def test_merge_idempotence_and_preservation(self):
        self.functions[0].update(status='analyzing', recovered_name='load_car', notes='human note')
        merged, missing = merge_inventory(inventory(), self.rows, 'new.json')
        self.assertEqual(missing, [])
        self.assertEqual(merged, self.rows)
        smaller = inventory()
        smaller['functions'].pop()
        smaller['function_count'] = 1
        merged, missing = merge_inventory(smaller, self.rows, 'new.json')
        self.assertEqual(len(missing), 1)
        self.assertEqual(len(merged), 3)
        foreign = inventory()
        foreign['binary_sha256'] = 'b' * 64
        with self.assertRaises(ValueError): merge_inventory(foreign, self.rows, 'new.json')
        incomplete = inventory()
        incomplete['function_count'] += 1
        with self.assertRaises(ValueError): merge_inventory(incomplete, [], 'new.json')

    def test_duplicates_stale_sources_and_collapsed_output(self):
        self.markers()
        synchronize(self.rows, self.root)
        (self.root / 'game_src/unresolved/sample.cpp').unlink()
        self.assertTrue(any('stale source' in e for e in validate(self.rows, self.root)))
        self.rows.append(self.rows[0].copy())
        self.assertTrue(any('Duplicate ID' in e for e in validate(self.rows, self.root)))
        output = render(self.rows)
        self.assertIn('<details>', output)
        self.assertNotIn('<details open', output)

    def test_cli_stale_output_and_explicit_update(self):
        save_rows(self.rows, self.root)
        command = [sys.executable, str(ROOT / 'scripts/progress.py'), '--root', str(self.root)]
        result = subprocess.run(command + ['update'], capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(subprocess.run(command + ['check'], capture_output=True).returncode, 0)
        output = self.root / 'docs/Progress.md'
        output.write_text(output.read_text() + 'stale\n')
        self.assertNotEqual(subprocess.run(command + ['check'], capture_output=True).returncode, 0)
        before = (self.root / 'docs/functions.csv').read_bytes()
        result = subprocess.run(command + ['set', self.functions[0]['id'], '--status', 'implemented'], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(before, (self.root / 'docs/functions.csv').read_bytes())

    def test_boundaries(self):
        path = self.root / 'game_src/unresolved/sample.cpp'
        path.write_text('# include <godot_cpp/classes/node.hpp>\n#include <windows.h>\n')
        self.assertEqual(len(check(self.root)), 2)
        path.write_text('#include <vector>\n')
        self.assertEqual(check(self.root), [])
        (self.root / 'game_src/unresolved/cd3du_alloc.hpp').write_text('#pragma once\n')
        path.write_text('#include "game_src/unresolved/cd3du_alloc.hpp"\n')
        self.assertEqual(check(self.root), [])

if __name__ == '__main__': unittest.main()
