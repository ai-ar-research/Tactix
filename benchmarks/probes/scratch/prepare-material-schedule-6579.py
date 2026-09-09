#!/usr/bin/env python3
"""Approved cutover: C2 material lifecycle and Eliminate invitation."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260909-6571-material-state-build/manifest.json'
m = json.loads(authority.read_text())
parent = root / m['source']
for rel, digest in m['files'].items():
    assert sha(parent / rel) == digest, rel
source = root / '.claude/worktrees/runtime-material-schedule-6579'
c = root / 'benchmarks/runs/20260909-6579-material-schedule-build'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
design = root / 'benchmarks/reports/2026-09-09-material-tracker-design-and-touch-contract.md'
with (c / 'parent.json').open('x') as out:
    json.dump(dict(path=str(authority.relative_to(root)), sha256=sha(authority),
        design=dict(path=str(design.relative_to(root)), sha256=sha(design)),
        approval='User replied Yes to implementing the isolated material-tracker repair, subject to correctness, memory and Rank1–50 validation before promotion. Approval does not cover FlatLists, fuzz access or push.',
        stage='Canonical C2 flag predicates and clear/rearm lifecycle, plus Eliminate material admission and maximum-bound exit watermark. Public guards, shared producers and SP-SLOT-5 unchanged.'), out, indent=2)
    out.write('\n')
print(source)
