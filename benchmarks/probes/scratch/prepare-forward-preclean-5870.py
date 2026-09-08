#!/usr/bin/env python3
"""Create isolated preconnection-cleanup successor of immutable5849."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
m = json.loads((root / 'benchmarks/runs/20260908-5849-forward-caller/manifest.json').read_text())
parent = root / m['source']
for rel, digest in m['files'].items(): assert hashlib.sha256((parent / rel).read_bytes()).hexdigest() == digest
source = root / '.claude/worktrees/runtime-forward-preclean-5870'
c = root / 'benchmarks/runs/20260908-5870-forward-preclean'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
print(source)
