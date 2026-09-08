#!/usr/bin/env python3
"""Stage explicit audited repair evidence, excluding binaries/large traces/fuzz data."""
from pathlib import Path
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-sweep-small-validation.json'
record = json.loads(audit.read_text())
paths = ['PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md',
    'docs/scheduling-parity-ledger.md', str(audit.relative_to(root)),
    'benchmarks/reports/2026-09-08-sweep-small-environments.md',
    'benchmarks/reports/2026-09-08-sweep-small-main-ctest.log']
scratch = root / 'benchmarks/probes/scratch'
for number in range(5522, 5544):
    paths += [str(p.relative_to(root)) for p in scratch.glob('*' + str(number) + '.py')]
for name in ('reference-small-kitten-5525.c', 'reference-small-tactix-5525.cpp',
             'claude-periodic-fuzz-5539.txt'):
    paths.append(str((scratch / name).relative_to(root)))
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
for item in record['campaigns']:
    c = root / item['campaign']
    assert sha(c / 'manifest.json') == item['manifest_sha256']
    for folder, key in [('results', 'result_sha256'), ('provisional', 'provisional_sha256')]:
        for name, digest in item[key].items():
            p = c / folder / name
            assert sha(p) == digest
            paths.append(str(p.relative_to(root)))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    if '-5522-' in c.name:
        paths += [str(p.relative_to(root)) for p in c.glob('*.patch')]
        paths += [str(p.relative_to(root)) for p in (c / 'logs').glob('*.log')]
    if any('-' + str(n) + '-' in c.name for n in (5529, 5537)):
        paths += [str((c / name).relative_to(root)) for name in ('run.py', 'array.sbatch')]
        for suffix in ('prefix.tsv', 'boundaries.tsv', 'macro.tsv'):
            paths += [str(p.relative_to(root)) for p in (c / 'traces').glob('*.' + suffix)]
    if any('-' + str(n) + '-' in c.name for n in (5530, 5531, 5535)):
        paths += [str((c / name).relative_to(root)) for name in ('matrix.tsv', 'array.sbatch')]
assert all((root / name).is_file() for name in paths)
assert not any('periodic-fuzz/' in name or name.endswith(('-response.json', '-stderr.log')) for name in paths)
subprocess.run(['git', 'add', '-f', '--', *sorted(set(paths))], cwd=root, check=True)
print('Staged explicit audited paths:', len(set(paths)))
