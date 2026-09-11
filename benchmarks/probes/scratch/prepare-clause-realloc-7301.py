#!/usr/bin/env python3
"""Create one isolated source copy, preserving the submitted7267 pins."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
p = root / 'benchmarks/runs/20260911-7271-dense-realloc-build/manifest.json'
m = json.loads(p.read_text())
parent = root / m['source']
source = root / '.claude/worktrees/runtime-clause-realloc-7301'
assert not source.exists()
for rel, digest in m['files'].items():
    assert sha(parent / rel) == digest, rel
source.mkdir()
for name in ('src', 'include', 'tests'):
    shutil.copytree(parent / name, source / name)
shutil.copy2(parent / 'CMakeLists.txt', source / 'CMakeLists.txt')
for name in ('tools', 'benchmarks', 'DEVELOPMENT_PLAN.md'):
    assert (parent / name).is_symlink()
    (source / name).symlink_to((parent / name).readlink())
(source / 'build').mkdir()
print(source)
