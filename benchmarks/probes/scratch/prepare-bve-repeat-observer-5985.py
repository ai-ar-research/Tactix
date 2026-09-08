#!/usr/bin/env python3
"""Clone frozen observers only; later patches stay outside all submitted pins."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5958-bve-full-heap'
m = json.loads((c / 'manifest.json').read_text())
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
for old, new in [(m['candidate_source'], '.claude/worktrees/runtime-bve-repeat-heap-5985'),
                 (m['native_source'], 'tools/solvers/variants/kissat-bve-repeat-heap-5985')]:
    shutil.copytree(root / old, root / new, symlinks=True,
                    ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(root / 'benchmarks/runs/20260908-5985-bve-repeat-heap-controls/logs').mkdir(parents=True, exist_ok=False)
