#!/usr/bin/env python3
"""Locate the new Rank31 regression before assuming the known caller gap owns it."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
parent = runs / '20260908-5810-forward-unit-rank31-prefix'
authority = runs / '20260908-5830-forward-admission'
c = runs / '20260908-5845-forward-admission-rank31-prefix'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((parent / 'manifest.json').read_text())
a = json.loads((authority / 'manifest.json').read_text())
binary = root / 'build/pin-5832-forward-admission-trace/tactix'
assert sha(binary) == a['binaries'][str(binary.relative_to(root))]
for rel, digest in m['hashes'].items():
    assert sha(parent / rel) == digest
c.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'):
    (c / folder).mkdir()
shutil.copy2(binary, c / 'pins/candidate')
(c / 'run.py').write_text((parent / 'run.py').read_text().replace('tactix-5810-', 'tactix-5845-'))
batch = (parent / 'array.sbatch').read_text().replace(str(parent), str(c))
batch = '\n'.join('#SBATCH --job-name=5845-admission-rank31' if line.startswith('#SBATCH --job-name=') else line for line in batch.split('\n'))
(c / 'array.sbatch').write_text(batch)
m.update(parent_campaign=str(parent.relative_to(root)), candidate_authority=str(authority.relative_to(root)),
         candidate_source=a['source'], source_patch_sha256=sha(authority / 'cumulative.patch'),
         hypothesis='5830 raises Rank31 conflicts from1.403354x to2.106051x native;locate earliest divergence versus5798 and cached native before attributing the regression to missing post-C2 caller closure.',
         validation='Complete300K stream from0,expected diagnostic stop and scratch cleanup;compare every row and portable forward/BVE boundaries to both cached controls. No native refresh or acceptance timing.',
         test_consideration=a['test_consideration'],
         cached_controls=dict(candidate=dict(record=str((parent / 'results/candidate.json').relative_to(root)),
                                             sha256=sha(parent / 'results/candidate.json')),
                              kissat=m['cached_controls']['kissat']),
         runtime_selection=dict(path='benchmarks/runs/20260908-5838-forward-admission-ranks1-40/next-losses.json',
                                sha256=sha(runs / '20260908-5838-forward-admission-ranks1-40/next-losses.json')))
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', c / 'pins/candidate']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)
