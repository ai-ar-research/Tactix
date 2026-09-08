#!/usr/bin/env python3
"""Isolate exact-rank linear maximum selection in the irredundant schedule."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-inprocess-root-5430',
               root / '.claude/worktrees/runtime-vivify-max-5438', symlinks=True,
               ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260907-5438-vivify-max'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
