#!/usr/bin/env python3
"""Isolate a diagnostic-only root-to-forward-mark observer from frozen5798."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
authority = root / 'benchmarks/runs/20260908-5798-forward-unit/manifest.json'
m = json.loads(authority.read_text())
parent = root / m['source']
for rel,digest in m['files'].items(): assert hashlib.sha256((parent / rel).read_bytes()).hexdigest() == digest
source = root / '.claude/worktrees/runtime-forward-mark-observer-5817'
c = root / 'benchmarks/runs/20260908-5817-forward-mark-observer'
assert not source.exists() and not c.exists()
shutil.copytree(parent,source,symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))
c.mkdir(); (c / 'logs').mkdir()
print(source)
