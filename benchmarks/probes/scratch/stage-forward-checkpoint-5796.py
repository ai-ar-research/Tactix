#!/usr/bin/env python3
"""Stage successor-audited normal evidence without fuzz outputs or large streams."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-rank32-forward-checkpoint-validation.json'
hashes = json.loads(audit.read_text())['hashes']
paths = {str(audit.relative_to(root))}
local_large = []
for rel, digest in hashes.items():
    if rel.startswith(('build/', 'report-to-andrew/')): continue
    p = root / rel
    if '/traces/' in rel and p.stat().st_size >= 5 << 20:
        local_large.append(rel)
        continue
    assert hashlib.sha256(p.read_bytes()).hexdigest() == digest, rel
    paths.add(rel)
assert not any(p.endswith(('-response.json', '-stderr.log')) or '/periodic-fuzz/' in p for p in paths)
subprocess.run(['git', 'add', '-f', '--', *sorted(paths)], cwd=root, check=True)
print('Staged explicit audited paths:', len(paths))
print('Large hash-bound streams kept local:', len(local_large))
