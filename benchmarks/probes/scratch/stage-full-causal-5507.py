#!/usr/bin/env python3
"""Stage only audited owned normal evidence, never fuzz paths or mutable traces."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-full-causal-validation.json'
d = json.loads(audit.read_text())
paths = ['PROGRESS.md', str(audit.relative_to(root)),
         'benchmarks/reports/2026-09-08-full-sweep-and-causal-witnesses.md',
         'benchmarks/reports/2026-09-08-full-causal-main-ctest.log']
for n in range(5496, 5508):
    paths += [str(p.relative_to(root)) for p in (root / 'benchmarks/probes/scratch').glob('*' + str(n) + '.py')]
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
for item in d['campaigns']:
    c = root / item['campaign']
    for sub, key in (('results', 'result_sha256'), ('provisional', 'provisional_sha256')):
        for name, digest in item[key].items():
            p = c / sub / name
            assert sha(p) == digest
            paths.append(str(p.relative_to(root)))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    if any(('-' + str(n) + '-') in c.name for n in (5496, 5497, 5498, 5502, 5505)):
        paths += [str((c / name).relative_to(root)) for name in ('run.py', 'array.sbatch')]
    if item['final_records'] == 2:
        for suffix in ('imply.tsv', 'binary.tsv', 'formula.tsv', 'factor.tsv'):
            paths += [str(p.relative_to(root)) for p in (c / 'traces').glob('*.' + suffix)]
assert all((root / p).is_file() for p in paths)
assert not any('fuzz' in p for p in paths)
subprocess.run(['git', 'add', '-f', '--', *sorted(set(paths))], cwd=root, check=True)
print('Staged explicit audited paths:', len(set(paths)))
