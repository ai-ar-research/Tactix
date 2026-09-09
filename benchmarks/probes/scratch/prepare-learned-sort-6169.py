#!/usr/bin/env python3
"""Clone the frozen observer-neutral current source for a private sort repair."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260909-6154-bve-capacity-build/summary.json'
b = json.loads(authority.read_text())
parent = root / b['source']
for rel, digest in b['files'].items():
    assert sha(parent / rel) == digest, rel
source = root / '.claude/worktrees/runtime-learned-sort-6169'
c = root / 'benchmarks/runs/20260909-6169-learned-sort-build'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
print(source)
