#!/usr/bin/env python3
"""Retarget the validated read-only observer to the differing sift subtree."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5949-bve-heap-lifecycle'
c = root / 'benchmarks/runs/20260908-5972-bve-sift-ancestry'
m = json.loads((parent / 'manifest.json').read_text())
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['hashes'].items():
    assert sha(parent / rel) == digest, rel
assert not c.exists()
subjects = '225614,127295,129082,40787,107019,41090,60705,89339'
assert len(set(subjects.split(','))) == 8
c.mkdir()
for name in ('pins', 'logs', 'results', 'traces'):
    (c / name).mkdir()
for p in (parent / 'pins').iterdir():
    shutil.copy2(p, c / 'pins' / p.name)
runner = (parent / 'run.py').read_text().replace('tactix-5949-', 'tactix-5972-')
old = "env['SAT_BVE_HEAP_SUBJECTS'] = '225614,225641,225620,89339'"
assert runner.count(old) == 1
runner = runner.replace(old, "env['SAT_BVE_HEAP_SUBJECTS'] = '" + subjects + "'")
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('5949-bve-heap-lifecycle', '5972-bve-sift-ancestry'))
evidence = root / 'benchmarks/runs/20260908-5958-bve-full-heap/summary.json'
m.update(parent_campaign=str(parent.relative_to(root)), subjects=subjects.split(','),
    prior_heap_evidence=dict(path=str(evidence.relative_to(root)), sha256=sha(evidence)),
    hypothesis='The first different sift choice is at index278: right child129082 in Tactix versus127295 in native. Trace both and adjacent displaced ancestors back to their first different heap mutation.',
    validation='Exact ordinary300K traces, partial proof bytes and complete BVE attempt journals must replay5949 before interpreting selected rows. Native rerun collects previously missing causal fields only; fixed runtime denominator unchanged.',
    test_consideration='No source or observer change: reuse actual native construction/reset controls and full replay validated5949. Existing parser has seven negative fixtures; manifest enforces eight unique subjects and runner syntax. Interpret native pre-push stage asymmetry explicitly.',
    limitation='Read-only diagnostic on retained5900, separate from5962 repair. No claimed improvement, promotion, or fuzz activity.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', *(c / 'pins').iterdir()]}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
