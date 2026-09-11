#!/usr/bin/env python3
"""Preserve7333 and isolate fixes for the inherited Debug failures."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260911-7335-root-watermark-build/manifest.json'
manifest = json.loads(authority.read_text())
parent = root / manifest['source']
source = root / '.claude/worktrees/runtime-debug-contracts-7339'
campaign = root / 'benchmarks/runs/20260911-7339-debug-contracts-prepare'
assert not source.exists() and not campaign.exists()
for rel, digest in manifest['files'].items(): assert sha(parent / rel) == digest, rel
source.mkdir()
for name in ('src', 'include', 'tests', 'examples'):
    shutil.copytree(parent / name, source / name)
shutil.copy2(parent / 'CMakeLists.txt', source / 'CMakeLists.txt')
for name in ('tools', 'benchmarks', 'DEVELOPMENT_PLAN.md'):
    assert (parent / name).is_symlink()
    (source / name).symlink_to((parent / name).readlink())
(source / 'build').mkdir()
for rel, digest in manifest['files'].items(): assert sha(source / rel) == digest, rel
campaign.mkdir()
with (campaign / 'manifest.json').open('x') as out:
    json.dump(dict(parent_manifest=str(authority.relative_to(root)), parent_sha256=sha(authority),
                   source=str(source.relative_to(root)), parent_files=manifest['files'],
                   helper_sha256=sha(Path(__file__)),
                   scope='Repair collector connection-state ownership; name proof fixtures before inference; preserve strict optimized runtime diagnostic gate while separately checking Debug no-op stubs. No rule guard, cadence or promotion.'), out, indent=2)
print(source)
