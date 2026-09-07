#!/usr/bin/env python3
"""List (or explicitly stage) this checkpoint's small durable evidence only."""
import argparse
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--stage', action='store_true')
args = parser.parse_args()
paths = {root / p for p in ('PROGRESS.md', 'docs/implementation-notes.md',
                            'docs/scheduling-parity-ledger.md')}
paths.update((root / 'benchmarks/reports').glob('2026-09-07-dense-entry-*'))
paths.update(p for p in (root / 'benchmarks/probes/scratch').iterdir()
             if re.search(r'(538[4-9]|539[0-5])\.(py|cpp|c|txt)$', p.name))
for number in (5380, 5383, *range(5384, 5396)):
    c = next((root / 'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths.update(p for p in c.iterdir() if p.is_file()
                 and p.suffix in ('.json', '.patch', '.tsv', '.sbatch'))
    paths.update((c / 'results').glob('*.json'))
    paths.update(p for p in (c / 'pins').iterdir() if p.is_file()
                 and p.suffix in ('.py', '.patch')) if (c / 'pins').exists() else None
    paths.update((c / 'profiles').glob('*.txt'))
    paths.update((c / 'logs').glob('*.tsv'))
    if number in (5387, 5391):
        paths.update(p for p in (c / 'logs').iterdir() if p.is_file())
assert all(p.is_file() and p.stat().st_size < 10_000_000 for p in paths)
relative = sorted(str(p.relative_to(root)) for p in paths)
print(f'{len(relative)} exact paths; {sum(p.stat().st_size for p in paths)} bytes')
for p in relative:
    print(p)
if args.stage:
    subprocess.run(['git', 'add', '-f', '--', *relative], cwd=root, check=True)
