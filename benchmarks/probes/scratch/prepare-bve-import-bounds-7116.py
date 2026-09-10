#!/usr/bin/env python3
"""Preserve frozen7110 and isolate compiler-range/test-comparison closure."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260910-7110-bve-import-build/manifest.json'
m = json.loads(authority.read_text()); parent = root / m['source']
for rel, digest in m['files'].items(): assert sha(parent / rel) == digest, rel
source = root / '.claude/worktrees/runtime-bve-import-bounds-7116'
campaign = root / 'benchmarks/runs/20260910-7116-bve-import-bounds-build'
assert not source.exists() and not campaign.exists()
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(campaign / 'logs').mkdir(parents=True)
with (campaign / 'parent.json').open('x') as stream:
    json.dump(dict(authority=str(authority.relative_to(root)), sha256=sha(authority),
        scope='Re-establish existing bounded XOR signature range at its consumer; unambiguous vector trail snapshot; boundary regression. No gate, scheduling, proof or API change.',
        inherited_approval='Approved BVE data-path prototype; stage2 mapping/index fusion7110 retained.'), stream, indent=2)
print(source)
