#!/usr/bin/env python3
"""Isolate the source-backed no-gates Congruence return-path repair."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json'
m = json.loads(authority.read_text())
parent = root / m['source']
for rel, digest in m['files'].items(): assert sha(parent / rel) == digest, rel
evidence = root / 'benchmarks/runs/20260909-6651-rank49-congruence-stages/summary-6653.json'
s = json.loads(evidence.read_text())
assert s['full_own_replay']
for rel, digest in s['hashes'].items(): assert sha(root / rel) == digest, rel
source = root / '.claude/worktrees/runtime-congruence-empty-gates-6654'
c = root / 'benchmarks/runs/20260909-6654-congruence-empty-gates-build'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
with (c / 'parent.json').open('x') as out:
    json.dump(dict(path=str(authority.relative_to(root)), sha256=sha(authority),
        evidence=dict(path=str(evidence.relative_to(root)), sha256=sha(evidence)),
        scope='Experimental removal of the gates.empty early return: binary-only equivalence rounds must reach the existing representative-aware forward cleanup. Public guards/instances, rule semantics, proof primitives, unit-only lazy cleanup and production remain unchanged.',
        test_consideration='New fixed public/direct rule cases: no gates with/without binary equivalence, units known at entry versus derived in the round, duplicate copies, both proof modes and import order. Parent-negative comparison precedes candidate suites. No fuzz authority or promotion implied.'), out, indent=2)
    out.write('\n')
print(source)
