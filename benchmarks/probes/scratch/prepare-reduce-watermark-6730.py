#!/usr/bin/env python3
"""Isolate the source-backed historical Reduce watermark repair."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260909-6654-congruence-empty-gates-build/manifest.json'
m = json.loads(authority.read_text())
parent = root / m['source']
for rel, digest in m['files'].items(): assert sha(parent / rel) == digest, rel
evidence = root / 'benchmarks/runs/20260909-6726-rank46-collect-boundary/summary-6729.json'
s = json.loads(evidence.read_text())
for rel, digest in s['hashes'].items(): assert sha(root / rel) == digest, rel
source = root / '.claude/worktrees/runtime-reduce-watermark-6730'
c = root / 'benchmarks/runs/20260909-6730-reduce-watermark-build'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
with (c / 'parent.json').open('x') as out:
    json.dump(dict(path=str(authority.relative_to(root)), sha256=sha(authority),
        evidence=dict(path=str(evidence.relative_to(root)), sha256=sha(evidence)),
        scope='Retain the historical generation-local Reduce collection boundary when its clause becomes irredundant. Advance next-reduction metadata separately; forward it through implementation-only packing and recompute after genuine collection. No guard, instance, proof primitive, scheduling admission or production change.',
        test_consideration='Parent-negative fixed tests for promoted boundary cursor reset and true-prefix retention, root/nonroot collection, garbage and aliases, repeated reduction and packing lifecycle. Native caller fixture and three full independent suites; certificates, B/C and causal/runtime gates follow. No fuzz or promotion authority.'), out, indent=2)
    out.write('\n')
print(source)
