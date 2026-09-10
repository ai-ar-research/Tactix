#!/usr/bin/env python3
"""Create a source-identical review copy; preserve failed source and binaries."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260910-7087-bve-views-build/manifest.json'
m = json.loads(authority.read_text())
source = root / m['source']
for rel, digest in m['files'].items():
    assert sha(source / rel) == digest, rel
target = root / '.claude/worktrees/runtime-bve-fixture-repair-7100'
assert not target.exists()
shutil.copytree(source, target, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
print(target)
