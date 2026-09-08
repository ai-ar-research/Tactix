#!/usr/bin/env python3
"""Isolate a behavior-preserving hot watch-metadata write removal."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-rephase-root-5417',
               root / '.claude/worktrees/runtime-watch-truncate-5426', symlinks=True,
               ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260907-5426-watch-truncate'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
