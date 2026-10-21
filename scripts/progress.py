"""Generate, validate and explicitly advance the reverse-engineering registry."""
import argparse
import sys
from tracking import ROOT, STATUSES, aggregate_classes, load_rows, render, save_rows, synchronize, validate

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=__import__('pathlib').Path, default=ROOT)
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('update')
    commands.add_parser('check')
    setter = commands.add_parser('set')
    setter.add_argument('id')
    setter.add_argument('--status', choices=STATUSES, required=True)
    setter.add_argument('--evidence')
    args = parser.parse_args()
    rows = load_rows(args.root)
    if args.command == 'set':
        row = next((r for r in rows if r['id'] == args.id), None)
        if row is None or row['kind'] != 'function':
            raise ValueError('Select a known function; class status is derived')
        if args.status == 'verified' and not args.evidence:
            raise ValueError('Explicit validation evidence is required when setting verified')
        row['status'] = args.status
        if args.evidence is not None:
            row['evidence'] = args.evidence
    if args.command != 'check':
        synchronize(rows, args.root)
    errors = validate(rows, args.root)
    if not rows:
        errors.append('Empty registry; import an inventory first')
    output = args.root / 'docs/Progress.md'
    expected = render(rows) if not errors else ''
    if args.command == 'check' and (not output.exists() or output.read_text(encoding='utf-8') != expected):
        errors.append('Progress.md is stale; run progress.py update')
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    if args.command != 'check':
        save_rows(rows, args.root)
        output.write_text(expected, encoding='utf-8', newline='\n')
    print(f'{args.command}: {len(rows)} registry rows validated')
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
