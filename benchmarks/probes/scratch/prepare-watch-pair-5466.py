#!/usr/bin/env python3
"""Isolate two-word append within existing watch-slice capacity."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
shutil.copytree(root/'.claude/worktrees/runtime-bound-advance-5459',
               root/'.claude/worktrees/runtime-watch-pair-5466',symlinks=True,
               ignore=shutil.ignore_patterns('build','.git','__pycache__'))
c=root/'benchmarks/runs/20260907-5466-watch-pair'
c.mkdir(exist_ok=False);(c/'logs').mkdir()
