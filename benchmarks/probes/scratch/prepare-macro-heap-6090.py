#!/usr/bin/env python3
"""Fresh observer-only clones; submitted sources remain immutable."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
for old,new,subdir in (
    ('.claude/worktrees/runtime-reduce-root-lock-trace-6054','.claude/worktrees/runtime-macro-heap-6090','src/diagnostics'),
    ('tools/solvers/variants/kissat-learned-stage-6030','tools/solvers/variants/kissat-macro-heap-6090','src')):
    shutil.copytree(root/old,root/new,symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))
    shutil.copy2(Path(__file__).with_name('macro-heap-6090.h'),root/new/subdir/'macro-heap-6090.h')
(root/'benchmarks/runs/20260908-6090-macro-heap-controls/logs').mkdir(parents=True,exist_ok=False)
