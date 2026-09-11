#!/usr/bin/env python3
"""Isolate a compile-time-only census; never edit submitted7302 source/pins."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
p = root/'benchmarks/runs/20260911-7302-clause-realloc-build/manifest.json'
m = json.loads(p.read_text()); parent = root/m['source']
source = root/'.claude/worktrees/runtime-root-census-7322'
assert not source.exists()
for rel,digest in m['files'].items(): assert sha(parent/rel) == digest,rel
source.mkdir()
for name in ('src','include','tests','examples'):shutil.copytree(parent/name,source/name)
shutil.copy2(parent/'CMakeLists.txt',source/'CMakeLists.txt')
for name in ('tools','benchmarks','DEVELOPMENT_PLAN.md'):
    assert (parent/name).is_symlink()
    (source/name).symlink_to((parent/name).readlink())
(source/'build').mkdir()
print(source)
