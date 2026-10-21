"""Shared inventory model. Only the registry stores human progress decisions."""
from __future__ import annotations
import csv
import html
import re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIELDS = 'id binary_sha256 kind address original_name class_id recovered_name category scope status confidence source_path source_symbol evidence notes'.split()
STATUSES = ['not_started', 'analyzing', 'understood', 'implemented', 'verified']
CATEGORIES = {'game', 'library', 'thunk', 'unknown'}
SCOPES = {'required', 'excluded', 'undecided'}
CONFIDENCES = {'unknown', 'hypothesis', 'high', 'confirmed'}
MARKER = re.compile(r'^\s*//\s*RE:\s*(\S+)\s*$', re.M)

def load_rows(root=ROOT):
    path = root / 'docs/functions.csv'
    if not path.exists():
        return []
    with path.open(encoding='utf-8', newline='') as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != FIELDS:
            raise ValueError('Unexpected functions.csv columns')
        return list(reader)

def save_rows(rows, root=ROOT):
    path = root / 'docs/functions.csv'
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix('.csv.tmp')
    with temporary.open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS, lineterminator='\n')
        writer.writeheader()
        writer.writerows(sorted(rows, key=lambda row: row['id']))
    temporary.replace(path)

def scan_sources(root=ROOT, include_stubs=True):
    mappings = defaultdict(list)
    for directory in ('game_src', 'platform'):
        for path in sorted((root / directory).rglob('*')):
            if path.suffix not in {'.hpp', '.h', '.cpp', '.cc'} or 'third_party' in path.parts:
                continue
            for match in MARKER.finditer(path.read_text(encoding='utf-8')):
                mappings[match[1]].append(path.relative_to(root).as_posix())
            if include_stubs:
                for match in re.finditer(r'^\s*//\s*RE-STUB:\s*(\S+)\s*$', path.read_text(encoding='utf-8'), re.M):
                    mappings[match[1]].append(path.relative_to(root).as_posix())
    return {key: sorted(set(paths)) for key, paths in mappings.items()}

def synchronize(rows, root=ROOT):
    mappings = scan_sources(root)
    for row in rows:
        if row['kind'] == 'function':
            row['source_path'] = ';'.join(mappings.get(row['id'], []))
    aggregate_classes(rows)

def aggregate_classes(rows):
    members = defaultdict(list)
    for row in rows:
        if row['kind'] == 'function' and row['class_id']:
            members[row['class_id']].append(row)
    for row in rows:
        if row['kind'] != 'class':
            continue
        required = [r for r in members[row['id']] if r['scope'] == 'required']
        row['status'] = min((r['status'] for r in required), key=STATUSES.index) if required else 'not_started'

def validate(rows, root=ROOT):
    errors = []
    implementations = scan_sources(root, include_stubs=False)
    by_id = {}
    for row in rows:
        if row['id'] in by_id:
            errors.append('Duplicate ID: ' + row['id'])
        by_id[row['id']] = row
    mappings = scan_sources(root)
    for key in mappings:
        if key not in by_id or by_id[key]['kind'] != 'function':
            errors.append('Unknown/non-function source marker: ' + key)
    hashes = {r['binary_sha256'] for r in rows}
    if len(hashes) > 1:
        errors.append('Registry mixes binary identities')
    for row in rows:
        key = row['id']
        for field, choices in [('kind', {'class', 'function'}), ('status', set(STATUSES)), ('scope', SCOPES), ('category', CATEGORIES), ('confidence', CONFIDENCES)]:
            if row[field] not in choices:
                errors.append(f'{key}: invalid {field}')
        if not re.fullmatch(r'[0-9a-f]{64}', row['binary_sha256']):
            errors.append(f'{key}: invalid hash')
        if row['id'] != f"{row['binary_sha256']}:{row['kind']}:{row['address'].lower()}":
            errors.append(f'{key}: unstable identity')
        owner = by_id.get(row['class_id'])
        if row['class_id'] and (not owner or owner['kind'] != 'class'):
            errors.append(f'{key}: invalid class reference')
        expected = ';'.join(mappings.get(key, []))
        if row['kind'] == 'function' and row['source_path'] != expected:
            errors.append(f'{key}: stale source mapping')
        if row['kind'] == 'function' and row['status'] in {'implemented', 'verified'} and key not in implementations:
            errors.append(f'{key}: implementation requires source marker')
        if row['status'] == 'verified' and row['kind'] == 'function' and not row['evidence'].strip():
            errors.append(f'{key}: verified requires evidence')
        if row['scope'] == 'excluded' and (row['category'] not in {'library', 'thunk'} or not row['evidence']):
            errors.append(f'{key}: exclusion requires library/thunk evidence')
    if not any('invalid status' in error for error in errors):
        copies = [r.copy() for r in rows]
        aggregate_classes(copies)
        for before, after in zip(rows, copies):
            if before['status'] != after['status']:
                errors.append(before['id'] + ': stale class aggregate')
    return errors

def cell(value):
    return html.escape(value).replace('|', '&#124;').replace('\n', '<br>')

def render(rows):
    functions = [r for r in rows if r['kind'] == 'function']
    classes = [r for r in rows if r['kind'] == 'class']
    required = [r for r in functions if r['scope'] == 'required']
    counts = Counter(r['status'] for r in functions)
    def rate(count, total):
        return f'{count}/{total} ({count / total:.1%})' if total else '0/0 (not yet established)'
    out = ['# Reverse Engineering Progress', '', '<!-- Generated by scripts/progress.py; edit functions.csv instead. -->', '',
           f'Functions: **{len(functions)}**. Known classes/types: **{len(classes)}**.', '',
           '**Undecided scope: ' + str(sum(r['scope'] == 'undecided' for r in functions)) + ' functions. These remain unresolved work, not completed exclusions.**', '',
           'Classified functions: ' + rate(sum(r['category'] != 'unknown' for r in functions), len(functions)), '',
           'Required implemented (including verified): ' + rate(sum(r['status'] in {'implemented', 'verified'} for r in required), len(required)), '',
           'Required verified: ' + rate(sum(r['status'] == 'verified' for r in required), len(required)), '',
           'Unassigned function ownership: ' + str(sum(not r['class_id'] for r in functions)), '',
           '| Function status | Count |', '| --- | ---: |']
    out += [f'| {status} | {counts[status]} |' for status in STATUSES]
    out += ['', 'Coverage is the current Ghidra inventory, not proof of all original source classes. Class status is the minimum status of known required members; an empty required set is not_started. Unknown members and undecided functions are not implied complete.', '',
            'The bootstrap triangle and host interfaces are new infrastructure and do not count as recovered game functions.', '']
    members = defaultdict(list)
    for row in functions:
        if row['class_id']:
            members[row['class_id']].append(row)
    out += ['<details>', f'<summary>Class member implementation checklist ({len(classes)})</summary>', '',
            '| Class | Known members | Required | Implemented or verified | Verified | Undecided | Source |',
            '| --- | ---: | ---: | ---: | ---: | ---: | --- |']
    for cls in sorted(classes, key=lambda row: row['original_name']):
        known = members[cls['id']]
        values = [cell(cls['original_name']), str(len(known)), str(sum(r['scope'] == 'required' for r in known)),
                  str(sum(r['status'] in {'implemented', 'verified'} for r in known)),
                  str(sum(r['status'] == 'verified' for r in known)),
                  str(sum(r['scope'] == 'undecided' for r in known)), cell(cls['source_path'])]
        out.append('| ' + ' | '.join(values) + ' |')
    out += ['', '</details>', '']
    groups = defaultdict(list)
    for row in rows:
        paths = row['source_path'].split(';')[0].split('/')
        subsystem = paths[1] if len(paths) > 2 and paths[0] == 'game_src' else 'unresolved'
        groups[(subsystem, row['kind'], row['category'])].append(row)
    for group, entries in sorted(groups.items()):
        out += ['<details>', f'<summary>{" / ".join(group)} ({len(entries)})</summary>', '',
                '| Address | Original name | Class ID | Recovered name | Scope | Status | Confidence | Source | Evidence |',
                '| --- | --- | --- | --- | --- | --- | --- | --- | --- |']
        for row in sorted(entries, key=lambda r: r['id']):
            out.append('| ' + ' | '.join(cell(row[k]) for k in ['address', 'original_name', 'class_id', 'recovered_name', 'scope', 'status', 'confidence', 'source_path', 'evidence']) + ' |')
        out += ['', '</details>', '']
    return '\n'.join(out)
