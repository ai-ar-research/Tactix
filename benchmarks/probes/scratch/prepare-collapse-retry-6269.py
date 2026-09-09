#!/usr/bin/env python3
"""Create a separately pinned successor for the observed Vivify collapse discrepancy."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260909-6231-substitute-retry-build/manifest.json'
m = json.loads(authority.read_text()); parent = root / m['source']
for rel, digest in m['files'].items(): assert sha(parent / rel) == digest
source = root / '.claude/worktrees/runtime-vivify-binary-retry-6269'
c = root / 'benchmarks/runs/20260909-6269-vivify-binary-retry-build'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
(c / 'parent.json').write_text(json.dumps(dict(path=str(authority.relative_to(root)), sha256=sha(authority)), indent=2)+'\n')
print(source)
