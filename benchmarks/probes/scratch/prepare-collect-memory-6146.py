#!/usr/bin/env python3
"""Copy verified current source before adding diagnostic GC checkpoints."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
authority = root / 'benchmarks/runs/20260908-6136-substitute-journal/manifest.json'
m = json.loads(authority.read_text())
parent = root / m['source']
base = json.loads((root / 'benchmarks/runs/20260908-6127-publication-diagnostic-builds/summary.json').read_text())
expected = dict(base['source_sha256'], **m['files'])
for rel, digest in expected.items():
    assert sha(parent / rel) == digest, rel
source = root / '.claude/worktrees/runtime-collect-memory-6146'
c = root / 'benchmarks/runs/20260908-6146-collect-memory-build'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
print(source)
