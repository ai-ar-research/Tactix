#!/usr/bin/env python3
"""Isolate the policy-owned Lucky root-publication repair from frozen7339."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
authority=root/'benchmarks/runs/20260911-7340-debug-contracts-build/manifest.json'
m=json.loads(authority.read_text());parent=root/m['source']
source=root/'.claude/worktrees/runtime-lucky-roots-7365';c=root/'benchmarks/runs/20260911-7365-lucky-roots-prepare'
assert not source.exists() and not c.exists()
for rel,digest in m['files'].items():assert sha(parent/rel)==digest,rel
source.mkdir();c.mkdir()
for name in ('src','include','tests','examples'):shutil.copytree(parent/name,source/name)
shutil.copy2(parent/'CMakeLists.txt',source/'CMakeLists.txt')
for name in ('tools','benchmarks','DEVELOPMENT_PLAN.md'):
    assert (parent/name).is_symlink();(source/name).symlink_to((parent/name).readlink())
(source/'build').mkdir()
test=Path(__file__).with_name('lucky-root-consumer-7364.cpp')
shutil.copy2(test,source/'tests/test_lucky_roots.cpp')
with (c/'manifest.json').open('x') as out:json.dump(dict(parent_manifest=str(authority.relative_to(root)),
    parent_sha256=sha(authority),source=str(source.relative_to(root)),parent_files=m['files'],
    test_sha256=sha(test),helper_sha256=sha(Path(__file__)),
    scope='Both DefaultPolicy Lucky calls consume their newly proved root suffix before any subsequent scheduling. No rule/kernel mutation, new threshold, hidden filtering or production promotion.'),out,indent=2)
print(source)
