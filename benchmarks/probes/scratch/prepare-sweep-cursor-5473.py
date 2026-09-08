#!/usr/bin/env python3
"""Isolate the Sweep ledger membership optimization from submitted pins."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-watch-pair-5466',
               root / '.claude/worktrees/runtime-sweep-cursor-5473', symlinks=True,
               ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260908-5473-sweep-cursor'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
