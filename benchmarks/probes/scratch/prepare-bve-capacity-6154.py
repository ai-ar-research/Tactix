#!/usr/bin/env python3
"""Create an isolated finer observer after checked rank9 excludes GC peak."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent_build = root / 'benchmarks/runs/20260908-6146-collect-memory-build/summary.json'
b = json.loads(parent_build.read_text())
parent = root / b['source']
for rel, digest in b['files'].items():
    assert hashlib.sha256((parent / rel).read_bytes()).hexdigest() == digest, rel
source = root / '.claude/worktrees/runtime-bve-capacity-6154'
c = root / 'benchmarks/runs/20260909-6154-bve-capacity-build'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
print(source)
