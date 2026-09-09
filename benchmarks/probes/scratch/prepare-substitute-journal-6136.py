#!/usr/bin/env python3
"""Isolate an observer-only successor; never edit a submitted source tree."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
build = root / 'benchmarks/runs/20260908-6127-publication-diagnostic-builds/summary.json'
b = json.loads(build.read_text())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
parent = root / b['source']
for rel, digest in b['source_sha256'].items():
    assert sha(parent / rel) == digest, rel
source = root / '.claude/worktrees/runtime-substitute-journal-6136'
c = root / 'benchmarks/runs/20260908-6136-substitute-journal'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
print(source)
