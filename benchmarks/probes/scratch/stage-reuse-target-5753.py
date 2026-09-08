#!/usr/bin/env python3
"""Stage only the audited checkpoint's text authorities; PDFs and pins stay local."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-reuse-target-fixed-reference-validation.json'
record = json.loads(audit.read_text())
paths = {str(audit.relative_to(root))}
for rel, digest in record['hashes'].items():
    if rel.startswith(('build/', 'report-to-andrew/')):
        continue
    p = root / rel
    assert hashlib.sha256(p.read_bytes()).hexdigest() == digest, rel
    paths.add(rel)
for number in range(5741, 5754):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.py'):
        paths.add(str(p.relative_to(root)))
assert not any('periodic-fuzz' in p for p in paths)
subprocess.run(['git', 'add', '-f', '--', *sorted(paths)], cwd=root, check=True)
print('Staged explicit audited paths:', len(paths))
