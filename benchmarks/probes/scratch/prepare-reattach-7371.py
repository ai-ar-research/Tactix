#!/usr/bin/env python3
"""Preserve Lucky7365 and isolate the inherited guidance-reattachment repair."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
authority=root/'benchmarks/runs/20260911-7369-lucky-roots-validation/manifest.json'
m=json.loads(authority.read_text());parent=root/m['source']
source=root/'.claude/worktrees/runtime-lucky-reattach-7371';c=root/'benchmarks/runs/20260911-7371-lucky-reattach-prepare'
assert not source.exists() and not c.exists()
for rel,digest in m['files'].items():assert sha(parent/rel)==digest
source.mkdir();c.mkdir()
for name in ('src','include','tests','examples'):shutil.copytree(parent/name,source/name)
shutil.copy2(parent/'CMakeLists.txt',source/'CMakeLists.txt')
for name in ('tools','benchmarks','DEVELOPMENT_PLAN.md'):(source/name).symlink_to((parent/name).readlink())
(source/'build').mkdir()
with (c/'manifest.json').open('x') as out:json.dump(dict(parent_manifest=str(authority.relative_to(root)),parent_sha256=sha(authority),parent_files=m['files'],source=str(source.relative_to(root)),helper_sha256=sha(Path(__file__)),scope='Do not append duplicate compact-index entries when attaching a fresh H to an already-used kernel. Preserve existing compact order and inverse; normal variable growth still appends new identities. No rule/guard or first-attachment scheduling change.'),out,indent=2)
print(source)
