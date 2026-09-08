#!/usr/bin/env python3
"""Stage only explicitly audited evidence and new diagnostic tooling."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-factor-peel-validation.json'
record = json.loads(audit.read_text())
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(8 << 20), b''): h.update(b)
    return h.hexdigest()
paths = ['PROGRESS.md', str(audit.relative_to(root)),
    'benchmarks/reports/2026-09-08-factor-candidate-peel.md',
    'benchmarks/reports/2026-09-08-factor-peel-main-ctest.log']
for n in range(5580, 5592):
    paths += [str(p.relative_to(root)) for p in (root / 'benchmarks/probes/scratch').glob('*' + str(n) + '.py')]
for item in record['campaigns']:
    c = root / item['campaign']
    assert sha(c / 'manifest.json') == item['manifest_sha256']
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    for folder, key in [('results', 'result_sha256'), ('provisional', 'provisional_sha256')]:
        for name, digest in item[key].items():
            p = c / folder / name
            assert sha(p) == digest
            paths.append(str(p.relative_to(root)))
            if folder != 'results': continue
            r = json.loads(p.read_text())
            for name, digest in r.get('trace_sha256', {}).items():
                if not name.endswith(('.prefix.tsv', '.boundaries.tsv', '.macro.tsv', '.heap.tsv', '.events.tsv')): continue
                p = c / 'traces' / name
                assert sha(p) == digest
                paths.append(str(p.relative_to(root)))
    if any('-' + str(n) + '-' in c.name for n in (5580, 5582, 5585, 5588)):
        paths += [str((c / name).relative_to(root)) for name in ('run.py', 'array.sbatch')]
assert all((root / name).is_file() for name in paths)
assert not any('periodic-fuzz/' in name or name.endswith(('.decisions.tsv', '-response.json', '-stderr.log')) for name in paths)
subprocess.run(['git', 'add', '-f', '--', *sorted(set(paths))], cwd=root, check=True)
print('Staged explicit audited files:', len(set(paths)))
