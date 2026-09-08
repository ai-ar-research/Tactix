#!/usr/bin/env python3
"""Stage only audited normal evidence and parent-owned helpers, never fuzz output."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
audits=[root / 'benchmarks/reports/2026-09-08-forward-unit-exit-validation.json',
        root / 'benchmarks/reports/2026-09-08-forward-exit-owner-followup-validation.json']
hashes={}
paths={str(p.relative_to(root)) for p in audits}
for p in audits:hashes.update(json.loads(p.read_text())['hashes'])
for rel,digest in hashes.items():
    if rel.startswith(('build/','report-to-andrew/')):continue
    p=root / rel
    assert hashlib.sha256(p.read_bytes()).hexdigest()==digest,rel
    paths.add(rel)
for number in range(5754,5777):
    for p in (root / 'benchmarks/probes/scratch').glob('*'+str(number)+'.*'):
        if p.suffix in ('.py','.txt','.cpp','.c'):paths.add(str(p.relative_to(root)))
assert not any(p.endswith(('-response.json','-stderr.log')) or '/periodic-fuzz/' in p for p in paths)
subprocess.run(['git','add','-f','--',*sorted(paths)],cwd=root,check=True)
print('Staged explicit audited paths:',len(paths))
