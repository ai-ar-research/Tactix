#!/usr/bin/env python3
"""Isolate the source-backed root-propagation step before policy Rephase."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-imply-fast-5414',
                root / '.claude/worktrees/runtime-rephase-root-5417', symlinks=True,
                ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260907-5417-rephase-root'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
