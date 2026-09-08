#!/usr/bin/env python3
"""Stage precisely hash-bound normal checkpoint evidence, without pushing."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-forward-admission-final-validation.json'
m = json.loads(audit.read_text())
paths = {str(audit.relative_to(root)), str(Path(__file__).relative_to(root))}
for rel, digest in m['hashes'].items():
    p = root / rel
    assert p.stat().st_size < 5 << 20
    assert hashlib.sha256(p.read_bytes()).hexdigest() == digest, rel
    assert not rel.startswith(('tools/fuzz/', 'report-to-andrew/', '.claude/worktrees/'))
    assert p.read_bytes()[:4] != b'\x7fELF'
    paths.add(rel)
subprocess.run(['git', 'add', '-f', '--', *sorted(paths)], cwd=root, check=True)
print('Staged', len(paths), 'hash-bound normal checkpoint paths;no push')
