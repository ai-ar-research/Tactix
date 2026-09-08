#!/usr/bin/env python3
"""Create new diagnostic copies of the validated phase observer."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6008-clause-pose-series'
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert hashlib.sha256((c / rel).read_bytes()).hexdigest() == digest
for field, new, subdir in [('candidate_source', '.claude/worktrees/runtime-vivify-pose-6016', 'src/diagnostics'),
                          ('native_source', 'tools/solvers/variants/kissat-vivify-pose-6016', 'src')]:
    shutil.copytree(root / m[field], root / new, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
    shutil.copy2(Path(__file__).with_name('vivify-pose-6016.h'), root / new / subdir / 'vivify-pose-6016.h')
(root / 'benchmarks/runs/20260908-6016-vivify-pose-controls/logs').mkdir(parents=True, exist_ok=False)
