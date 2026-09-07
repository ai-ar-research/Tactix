#!/usr/bin/env python3
"""Make the existing one-shot snapshot reachable without the broad event observer."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
for before, after in [
    ('.claude/worktrees/runtime-window-observer-5399', '.claude/worktrees/runtime-direct-heap-5402'),
    ('tools/solvers/variants/kissat-window-observer-5399', 'tools/solvers/variants/kissat-direct-heap-5402'),
]:
    shutil.copytree(root / before, root / after, symlinks=True,
                    ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260907-5402-direct-heap-observer'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
