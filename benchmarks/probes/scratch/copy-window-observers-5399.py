#!/usr/bin/env python3
"""Isolate diagnostic-only stream filters from every submitted source/pin."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
for before, after in [
    ('.claude/worktrees/runtime-eliminate-dense-entry-5391', '.claude/worktrees/runtime-window-observer-5399'),
    ('tools/solvers/variants/kissat-retry-producers-5377', 'tools/solvers/variants/kissat-window-observer-5399'),
]:
    shutil.copytree(root / before, root / after, symlinks=True,
                    ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260907-5399-window-observers'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
