#!/usr/bin/env python3
"""Isolate an early-filter diagnostic observer; never mutate submitted pins."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
for before, after in [
    ('.claude/worktrees/runtime-direct-heap-5402', '.claude/worktrees/runtime-imply-observer-5412'),
    ('tools/solvers/variants/kissat-direct-heap-5402', 'tools/solvers/variants/kissat-imply-observer-5412'),
]:
    shutil.copytree(root / before, root / after, symlinks=True,
                    ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260907-5412-imply-observer'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
