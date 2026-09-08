#!/usr/bin/env python3
"""Stage explicit frozen repair/validation artifacts and current status only."""
from pathlib import Path
import hashlib
import json
import subprocess
root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-factor-support-validation.json'
record = json.loads(audit.read_text())
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(8 << 20), b''): h.update(b)
    return h.hexdigest()
paths = ['PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md',
    str(audit.relative_to(root)), 'benchmarks/reports/2026-09-08-factor-root-support-repair.md',
    'benchmarks/reports/2026-09-08-factor-support-main-ctest.log',
    'benchmarks/probes/scratch/reference-factor-root-support-5595.c']
for n in range(5592, 5610):
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
                if not name.endswith(('.prefix.tsv', '.boundaries.tsv', '.macro.tsv')): continue
                p = c / 'traces' / name
                assert sha(p) == digest
                paths.append(str(p.relative_to(root)))
    if '-5592-' in c.name:
        m = json.loads((c / 'manifest.json').read_text())
        for folder, key in [('', 'patches'), ('logs', 'logs')]:
            for name, digest in m[key].items():
                p = c / folder / name
                assert sha(p) == digest
                paths.append(str(p.relative_to(root)))
    if '-5597-' in c.name:
        paths += [str((c / name).relative_to(root)) for name in ('run.py', 'array.sbatch')]
    if any('-' + str(n) + '-' in c.name for n in (5598,5599,5604)):
        paths += [str((c / name).relative_to(root)) for name in ('array.sbatch', 'matrix.tsv')]
assert all((root / name).is_file() for name in paths)
assert not any('periodic-fuzz/' in name or name.endswith(('.decisions.tsv', '-response.json', '-stderr.log')) for name in paths)
subprocess.run(['git', 'add', '-f', '--', *sorted(set(paths))], cwd=root, check=True)
print('Staged explicit audited files:', len(set(paths)))
