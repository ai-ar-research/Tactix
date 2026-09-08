#!/usr/bin/env python3
"""Stage exact checkpoint evidence, excluding binaries, live logs and fuzz material."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-forward-unit-validation.json'
bound = json.loads(audit.read_text())['hashes']
paths = {str(audit.relative_to(root)), str(Path(__file__).relative_to(root))}
for rel, digest in bound.items():
    p = root / rel
    assert hashlib.sha256(p.read_bytes()).hexdigest() == digest, rel
    assert p.stat().st_size < 5 << 20, rel
    paths.add(rel)
campaign = root / 'benchmarks/runs/20260908-5806-forward-unit-BC-gate'
m = json.loads((campaign / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    p = campaign / rel
    assert hashlib.sha256(p.read_bytes()).hexdigest() == digest
    if p.suffix in ('.py','.patch','.sbatch','.tsv'):
        paths.add(str(p.relative_to(root)))
for rel in sorted(paths):
    assert not rel.startswith(('tools/fuzz/', 'report-to-andrew/'))
subprocess.run(['git','add','-f','--',*sorted(paths)], cwd=root, check=True)
print('Staged',len(paths),'exact normal-evidence paths;no push')
