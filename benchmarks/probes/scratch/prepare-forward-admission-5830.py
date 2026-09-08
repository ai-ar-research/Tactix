#!/usr/bin/env python3
"""Isolate C2's native admission producer from the frozen5798 baseline."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
authority = root / 'benchmarks/runs/20260908-5798-forward-unit/manifest.json'
m = json.loads(authority.read_text())
parent = root / m['source']
source = root / '.claude/worktrees/runtime-forward-admission-5830'
c = root / 'benchmarks/runs/20260908-5830-forward-admission'
assert not source.exists() and not c.exists()
for rel, digest in m['files'].items():
    assert hashlib.sha256((parent / rel).read_bytes()).hexdigest() == digest, rel
shutil.copytree(parent, source, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
c.mkdir()
(c / 'logs').mkdir()
print(source)
