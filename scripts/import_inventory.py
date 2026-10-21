"""Merge a complete export without discarding human progress or missing entries."""
import argparse
import json
import sys
from pathlib import Path
from tracking import FIELDS, ROOT, load_rows, save_rows

def merge_inventory(data, existing, evidence):
    digest = data['binary_sha256']
    if len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
        raise ValueError('Invalid binary hash')
    if any(row['binary_sha256'] != digest for row in existing):
        raise ValueError('Binary identity changed; use a separate registry')
    for kind in ('function', 'class'):
        if len(data[kind + 'es' if kind == 'class' else 'functions']) != data[kind + '_count']:
            raise ValueError('Incomplete inventory count')
    incoming = {}
    class_names = {row['original_name'].removeprefix('class ').removeprefix('struct '): row['id']
                   for row in existing if row['kind'] == 'class'}
    for item in data['classes']:
        address = item.get('rtti0_address', item.get('address', '')).lower()
        name = item['name']
        key = f'{digest}:class:{address}'
        class_names[name.removeprefix('class ').removeprefix('struct ')] = key
        incoming[key] = make_row(digest, 'class', address, name, evidence)
    for item in data['functions']:
        address = item['entry_point'].lower()
        row = make_row(digest, 'function', address, item['name'], evidence)
        owner = item.get('namespace', 'Global')
        row['class_id'] = class_names.get(owner, '')
        if item.get('is_thunk') is True:
            row.update(category='thunk', scope='excluded', notes='Ghidra isThunk flag')
        elif item.get('is_external') is True:
            row.update(category='library', scope='excluded', notes='External import: ' + item.get('library', ''))
        incoming[row['id']] = row
    expected = data['function_count'] + data['class_count']
    if len(incoming) != expected:
        raise ValueError('Duplicate inventory identity')
    old = {row['id']: row.copy() for row in existing}
    if len(old) != len(existing):
        raise ValueError('Duplicate registry identity')
    missing = sorted(old.keys() - incoming.keys())
    for key, row in incoming.items():
        if key in old:
            old[key]['original_name'] = row['original_name']
            if not old[key]['class_id']:
                old[key]['class_id'] = row['class_id']
        else:
            old[key] = row
    return sorted(old.values(), key=lambda row: row['id']), missing

def make_row(digest, kind, address, name, evidence):
    row = dict.fromkeys(FIELDS, '')
    row.update(id=f'{digest}:{kind}:{address}', binary_sha256=digest, kind=kind,
               address=address, original_name=name, category='unknown', scope='undecided',
               status='not_started', confidence='unknown', evidence=evidence)
    return row

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('export', type=Path)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--allow-missing', action='store_true', help='Retain missing entries and explicitly accept inventory shrinkage')
    args = parser.parse_args()
    evidence = args.export.resolve().relative_to(args.root.resolve()).as_posix()
    rows, missing = merge_inventory(json.loads(args.export.read_text(encoding='utf-8')), load_rows(args.root), evidence)
    if missing:
        print('Missing entries (retained):\n' + '\n'.join(missing), file=sys.stderr)
        if not args.allow_missing:
            return 1
    save_rows(rows, args.root)
    print(f'Imported {len(rows)} rows; {len(missing)} missing entries retained')
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, KeyError, OSError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
