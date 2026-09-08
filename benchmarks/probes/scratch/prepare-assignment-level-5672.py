#!/usr/bin/env python3
"""Isolate a bounded-maximum unit-cost optimization from submitted5649."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-propagation-cursor-repair-5647',
    root / '.claude/worktrees/runtime-assignment-level-5672', symlinks=True,
    ignore=shutil.ignore_patterns('build','.git','__pycache__'))
c = root / 'benchmarks/runs/20260908-5672-assignment-level'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
