#!/usr/bin/env python3
"""Isolate the explicitly approved BVE data-path prototype from frozen7030."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260910-7030-collect-retry-build/manifest.json'
m = json.loads(authority.read_text())
parent = root / m['source']
for rel, digest in m['files'].items():
    assert sha(parent / rel) == digest, rel
for rel, digest in m['binaries'].items():
    assert sha(root / rel) == digest, rel
source = root / '.claude/worktrees/runtime-bve-views-7087'
campaign = root / 'benchmarks/runs/20260910-7087-bve-views-build'
assert not source.exists() and not campaign.exists()
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(campaign / 'logs').mkdir(parents=True)
with (campaign / 'parent.json').open('x') as stream:
    json.dump(dict(authority=str(authority.relative_to(root)), sha256=sha(authority),
        approval='User explicitly answered Yes to the BVE data-path prototype after73a78fd8.',
        design='benchmarks/reports/2026-09-10-eliminate-lifecycle-cost.md',
        scope='Stage1 stable arena-offset antecedent slices with pooled root-filtered fallback. Preserve ordering/ticks/proofs/core/generator/N1. No kernel layout, FlatLists or scheduling change.',
        gate='Prove lifetime and snapshots in fixed tests; full suites and exact-work/certificate controls before runtime.20% complete-Eliminate target applies to the complete prototype, not a claimed stage1 gain.'), stream, indent=2)
    stream.write('\n')
print(source)
