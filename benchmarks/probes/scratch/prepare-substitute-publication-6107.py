#!/usr/bin/env python3
"""Clone the measured source and seal the actual-heap causal replay."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-6049-reduce-root-lock'
source = root / '.claude/worktrees/runtime-reduce-root-lock-6049'
target = root / '.claude/worktrees/runtime-substitute-publication-6107'
out = root / 'benchmarks/runs/20260908-6107-substitute-publication'
assert not target.exists() and not out.exists()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['files'].items():
    assert sha(source / rel) == digest, rel
causal = root / 'benchmarks/runs/20260908-6100-rank37-substitute-deactivation'
args = [causal / 'replay-controls/replay', causal / 'candidate.substitute-begin.tsv',
        causal / 'candidate.removal-order.txt', causal / 'kissat.removal-order.txt',
        causal / 'candidate.substitute-end.tsv', causal / 'kissat.substitute-end.tsv']
p = subprocess.run(list(map(str, args)), capture_output=True, text=True, check=True)
assert p.stdout.count('0 differing final positions') == 2
shutil.copytree(source, target, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(out / 'logs').mkdir(parents=True)
(out / 'causal-replay.json').write_text(json.dumps(dict(
    command=list(map(str, args)), stdout=p.stdout,
    hashes={str(f.relative_to(root)): sha(f) for f in args},
    deactivation_summary_sha256=sha(causal / 'summary.json'),
    conclusion='Both observed removal orders exactly reproduce their complete output heaps using actual Tactix heap code.'), indent=2) + '\n')
print(target)
