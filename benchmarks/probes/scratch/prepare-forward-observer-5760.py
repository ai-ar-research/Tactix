#!/usr/bin/env python3
"""Create an unsubmitted trace-only source for the forward-round exit cause."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-reuse-target-5741',
    root / '.claude/worktrees/runtime-forward-exit-observer-5760', symlinks=True,
    ignore=shutil.ignore_patterns('.git','build','__pycache__'))
c = root / 'benchmarks/runs/20260908-5760-forward-exit-observer'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
