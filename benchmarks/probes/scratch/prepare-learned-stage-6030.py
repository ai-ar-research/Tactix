#!/usr/bin/env python3
"""Create diagnostic-only copies without touching submitted6016 sources."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6020-vivify-pose'
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert hashlib.sha256((c / rel).read_bytes()).hexdigest() == digest
for field, new, subdir in [('candidate_source', '.claude/worktrees/runtime-learned-stage-6030', 'src/diagnostics'),
                          ('native_source', 'tools/solvers/variants/kissat-learned-stage-6030', 'src')]:
    shutil.copytree(root / m[field], root / new, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
    shutil.copy2(Path(__file__).with_name('learned-stage-6030.h'), root / new / subdir / 'learned-stage-6030.h')
(root / 'benchmarks/runs/20260908-6030-learned-stage-controls/logs').mkdir(parents=True, exist_ok=False)
