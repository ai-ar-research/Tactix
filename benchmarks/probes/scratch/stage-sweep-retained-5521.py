#!/usr/bin/env python3
"""Stage explicit audited repair evidence; exclude binaries, large traces and fuzz."""
from pathlib import Path
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-sweep-retained-validation.json'
record = json.loads(audit.read_text())
paths = ['PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md',
         'docs/scheduling-parity-ledger.md', str(audit.relative_to(root)),
         'benchmarks/reports/2026-09-08-sweep-retained-entry.md',
         'benchmarks/reports/2026-09-08-sweep-retained-main-ctest.log']
for number in range(5509, 5522):
    paths += [str(p.relative_to(root)) for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.py')]
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
for item in record['campaigns']:
    campaign = root / item['campaign']
    for folder, key in [('results', 'result_sha256'), ('provisional', 'provisional_sha256')]:
        for name, digest in item[key].items():
            path = campaign / folder / name
            assert sha(path) == digest
            paths.append(str(path.relative_to(root)))
    paths += [str(p.relative_to(root)) for p in campaign.glob('*.json')]
    if '-5510-' in campaign.name:
        paths += [str(p.relative_to(root)) for p in campaign.glob('*.patch')]
        paths += [str(p.relative_to(root)) for p in (campaign / 'logs').glob('*.log')]
    if '-5515-' in campaign.name:
        paths += [str((campaign / name).relative_to(root)) for name in ('run.py', 'array.sbatch')]
    if '-5516-' in campaign.name or '-5517-' in campaign.name:
        paths += [str((campaign / name).relative_to(root)) for name in ('matrix.tsv', 'array.sbatch')]
    if '-5505-' in campaign.name or '-5515-' in campaign.name:
        for suffix in ('binary.tsv', 'formula.tsv', 'prefix.tsv', 'boundaries.tsv'):
            paths += [str(p.relative_to(root)) for p in (campaign / 'traces').glob('*.' + suffix)]
assert all((root / name).is_file() for name in paths)
assert not any('fuzz' in name for name in paths)
subprocess.run(['git', 'add', '-f', '--', *sorted(set(paths))], cwd=root, check=True)
print('Staged explicit audited paths:', len(set(paths)))
