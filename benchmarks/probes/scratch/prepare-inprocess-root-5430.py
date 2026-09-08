#!/usr/bin/env python3
"""Isolate phase-entry root closure independently of the metadata ablation."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-rephase-root-5417',
               root / '.claude/worktrees/runtime-inprocess-root-5430', symlinks=True,
               ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260907-5430-inprocess-root'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
