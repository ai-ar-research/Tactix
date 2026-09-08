#!/usr/bin/env python3
"""Stage explicitly audited normal evidence; never scan independently owned fuzz."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-congruence-index-validation.json'
record = json.loads(audit.read_text())
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
paths = {'PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md',
    str(audit.relative_to(root)), 'benchmarks/reports/2026-09-08-congruence-index-cleanup.md',
    'benchmarks/reports/2026-09-08-congruence-index-main-ctest.log',
    'benchmarks/probes/scratch/reference-congruence-index-5618.c',
    'benchmarks/probes/scratch/current-congruence-index-5618.cpp',
    'benchmarks/probes/scratch/claude-congruence-index-repair-5623.txt',
    'benchmarks/probes/scratch/5623-congruence-index-repair/index_cleanup_diag.cpp',
    'benchmarks/runs/20260908-5623-congruence-index-repair/test_congruence_index_cleanup.diff'}
def include(p, digest=None):
    assert p.is_file(), p
    if digest is not None: assert sha(p) == digest, p
    paths.add(str(p.relative_to(root)))
for n in range(5618, 5641):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(n) + '.py'): include(p)
for rel, digest in record['fixed_authorities'].items(): include(root / rel, digest)
for number, name in ((5618, 'congruence-index-fixture'), (5619, 'congruence-index')):
    c = root / ('benchmarks/runs/20260908-' + str(number) + '-' + name)
    if number == 5618:
        for p in (c / 'logs').glob('*.log'): include(p)
    else:
        m = json.loads((c / 'manifest.json').read_text())
        for folder, key in (('', 'patches'), ('logs', 'logs')):
            for name, digest in m[key].items(): include(c / folder / name, digest)
for item in record['campaigns']:
    c = root / item['campaign']
    include(c / 'manifest.json', item['manifest_sha256'])
    for name, digest in item['summary_sha256'].items(): include(c / name, digest)
    # Campaign metadata and localization files are immutable once published.
    for p in c.glob('*.json'):
        if p.name not in item['summary_sha256']: include(p)
    for folder, key in (('results', 'result_sha256'), ('provisional', 'provisional_sha256')):
        for name, digest in item[key].items():
            p = c / folder / name
            include(p, digest)
            if folder != 'results': continue
            r = json.loads(p.read_text())
            for name, digest in r.get('trace_sha256', {}).items():
                small = name.endswith(('.prefix.tsv', '.boundaries.tsv', '.macro.tsv'))
                selected_formula = '-5613-' in c.name and name.endswith('.formula.tsv')
                if small or selected_formula: include(c / 'traces' / name, digest)
    if '-5626-' in c.name:
        m = json.loads((c / 'manifest.json').read_text())
        for folder, key in (('', 'patches'), ('logs', 'logs')):
            for name, digest in m[key].items(): include(c / folder / name, digest)
        for rel, digest in m['full_suite_logs'].items(): include(root / rel, digest)
    if any('-' + str(n) + '-' in c.name for n in (5611, 5613, 5627, 5634, 5639)):
        for name in ('run.py', 'array.sbatch'): include(c / name)
    if any('-' + str(n) + '-' in c.name for n in (5598, 5599, 5604, 5628, 5629)):
        for name in ('array.sbatch', 'matrix.tsv'): include(c / name)
assert all((root / name).is_file() for name in paths)
assert not any('periodic-fuzz/' in name or name.endswith(('.decisions.tsv', '-response.json', '-stderr.log')) for name in paths)
subprocess.run(['git', 'add', '-f', '--', *sorted(paths)], cwd=root, check=True)
print('Staged explicit audited files:', len(paths))
