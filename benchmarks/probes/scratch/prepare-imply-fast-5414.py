#!/usr/bin/env python3
"""Avoid per-heap-move variable selection in implication-only diagnostics."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
for before, after in [
    ('.claude/worktrees/runtime-imply-observer-5412', '.claude/worktrees/runtime-imply-fast-5414'),
    ('tools/solvers/variants/kissat-imply-observer-5412', 'tools/solvers/variants/kissat-imply-fast-5414'),
]:
    shutil.copytree(root / before, root / after, symlinks=True,
                    ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260907-5414-imply-fast'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
