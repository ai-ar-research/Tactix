#!/usr/bin/env python3
"""Stage explicitly audited normal campaign evidence and the isolated repair."""
from pathlib import Path
import hashlib
import json
import subprocess
root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-vivify-census-validation.json'
record = json.loads(audit.read_text())
paths = ['PROGRESS.md', 'docs/implementation-notes.md', str(audit.relative_to(root)),
    'benchmarks/reports/2026-09-08-vivify-unused-census.md',
    'benchmarks/reports/2026-09-08-vivify-census-main-ctest.log']
for number in range(5544, 5564):
    paths += [str(p.relative_to(root)) for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.py')]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for item in record['campaigns']:
    c = root / item['campaign']
    assert sha(c / 'manifest.json') == item['manifest_sha256']
    for folder, key in [('results', 'result_sha256'), ('provisional', 'provisional_sha256')]:
        for name, digest in item[key].items():
            p = c / folder / name
            assert sha(p) == digest
            paths.append(str(p.relative_to(root)))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    if '-5546-' in c.name:
        paths += [str(p.relative_to(root)) for p in c.glob('*.patch')]
        paths += [str(p.relative_to(root)) for p in (c / 'logs').glob('*.log')]
    if any('-' + str(n) + '-' in c.name for n in (5537, 5558)):
        paths += [str((c / name).relative_to(root)) for name in ('run.py', 'array.sbatch')]
        # An active arm may already have a growing trace file. Stage only
        # traces belonging to a completed result captured in this audit.
        for result_name in item['result_sha256']:
            result = json.loads((c / 'results' / result_name).read_text())
            for name, digest in result['trace_sha256'].items():
                if name.endswith(('.prefix.tsv', '.boundaries.tsv', '.macro.tsv')):
                    p = c / 'traces' / name
                    assert sha(p) == digest
                    paths.append(str(p.relative_to(root)))
    if any('-' + str(n) + '-' in c.name for n in (5544, 5550, 5551)):
        paths += [str((c / name).relative_to(root)) for name in ('matrix.tsv', 'array.sbatch')]
assert all((root / name).is_file() for name in paths)
assert not any('periodic-fuzz/' in name or name.endswith(('-response.json', '-stderr.log')) for name in paths)
subprocess.run(['git', 'add', '-f', '--', *sorted(set(paths))], cwd=root, check=True)
print('Staged explicit audited paths:', len(set(paths)))
