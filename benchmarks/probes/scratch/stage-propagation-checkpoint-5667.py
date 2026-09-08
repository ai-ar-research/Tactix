#!/usr/bin/env python3
"""Stage explicit audited normal authorities, never independently owned fuzz."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-propagation-cursor-validation.json'
record = json.loads(audit.read_text())
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
paths = {'PROGRESS.md', 'docs/implementation-notes.md', str(audit.relative_to(root)),
    'benchmarks/reports/2026-09-08-propagation-cursor.md',
    'benchmarks/reports/2026-09-08-propagation-cursor-main-ctest.log',
    'benchmarks/probes/scratch/claude-propagation-cursor-repair-5647.txt',
    'benchmarks/probes/scratch/claude-periodic-fuzz-5657.txt'}
def include(p, digest=None):
    assert p.is_file(), p
    if digest is not None: assert sha(p) == digest, p
    paths.add(str(p.relative_to(root)))
for n in range(5641, 5668):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(n) + '.py'): include(p)
for rel, digest in record['fixed_authorities'].items(): include(root / rel, digest)
for item in record['campaigns']:
    c = root / item['campaign']
    include(c / 'manifest.json', item['manifest_sha256'])
    for name, digest in item['summary_sha256'].items(): include(c / name, digest)
    for p in c.glob('*.json'):
        if p.name not in item['summary_sha256']: include(p)
    for folder, key in (('results', 'result_sha256'), ('provisional', 'provisional_sha256')):
        for name, digest in item[key].items():
            p = c / folder / name
            include(p, digest)
            if folder != 'results': continue
            r = json.loads(p.read_text())
            for name, digest in r.get('trace_sha256', {}).items():
                if name.endswith(('.prefix.tsv', '.boundaries.tsv', '.macro.tsv')):
                    include(c / 'traces' / name, digest)
    for name in ('run.py', 'array.sbatch', 'matrix.tsv'):
        if (c / name).is_file(): include(c / name)
    m = json.loads((c / 'manifest.json').read_text())
    if '-5649-' in c.name:
        for folder, key in (('', 'patches'), ('logs', 'logs')):
            for name, digest in m[key].items(): include(c / folder / name, digest)
        for rel, digest in m['review_evidence'].items(): include(root / rel, digest)
    for name, digest in m.get('pin_sha256', m.get('hashes', {})).items():
        p = c / name
        if p.suffix in ('.py', '.patch', '.sbatch', '.tsv'): include(p, digest)
assert all((root / name).is_file() for name in paths)
assert not any('periodic-fuzz/' in name or name.endswith(('.decisions.tsv', '.sweep.tsv', '-response.json', '-stderr.log')) for name in paths)
subprocess.run(['git', 'add', '-f', '--', *sorted(paths)], cwd=root, check=True)
print('Staged explicit audited files:', len(paths))
