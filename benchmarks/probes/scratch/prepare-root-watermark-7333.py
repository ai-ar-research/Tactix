#!/usr/bin/env python3
"""Isolate the logical-root watermark coupling; leave active 7330 gates intact."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
parent = root / '.claude/worktrees/runtime-root-fold-7330'
source = root / '.claude/worktrees/runtime-root-watermark-7333'
campaign = root / 'benchmarks/runs/20260911-7333-root-watermark-prepare'
assert not source.exists() and not campaign.exists()
files = {str(p.relative_to(parent)): sha(p)
         for folder in ('src', 'include', 'tests', 'examples')
         for p in sorted((parent / folder).rglob('*')) if p.is_file()}
files['CMakeLists.txt'] = sha(parent / 'CMakeLists.txt')
source.mkdir()
for name in ('src', 'include', 'tests', 'examples'):
    shutil.copytree(parent / name, source / name)
shutil.copy2(parent / 'CMakeLists.txt', source / 'CMakeLists.txt')
for name in ('tools', 'benchmarks', 'DEVELOPMENT_PLAN.md'):
    assert (parent / name).is_symlink()
    (source / name).symlink_to((parent / name).readlink())
(source / 'build').mkdir()
for rel, digest in files.items():
    assert sha(parent / rel) == sha(source / rel) == digest, rel
campaign.mkdir()
with (campaign / 'manifest.json').open('x') as out:
    json.dump(dict(parent=str(parent.relative_to(root)), source=str(source.relative_to(root)),
                   parent_files=files, helper_sha256=sha(Path(__file__)),
                   reason='Physical root compaction must not lower the logical fixed-unit watermark consumed by Eliminate and closure scheduling. Add actual-consumer regression, preserve the old prototype and its active preflight/builds; no benchmark submission or acceptance.'), out, indent=2)
print(source)
