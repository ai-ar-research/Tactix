#!/usr/bin/env python3
"""Clone frozen diagnostic sources; never edit submitted sources or pins."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5988-bve-repeat-heap'
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert hashlib.sha256((parent / rel).read_bytes()).hexdigest() == digest, rel
header = Path(__file__).with_name('clause-pose-series-6004.h')
for old, new, dest in [(m['candidate_source'], '.claude/worktrees/runtime-clause-pose-series-6004', 'src/diagnostics'),
                       (m['native_source'], 'tools/solvers/variants/kissat-clause-pose-series-6004', 'src')]:
    shutil.copytree(root / old, root / new, symlinks=True,
                    ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
    shutil.copy2(header, root / new / dest / header.name)
(root / 'benchmarks/runs/20260908-6004-clause-pose-series-controls/logs').mkdir(parents=True, exist_ok=False)
