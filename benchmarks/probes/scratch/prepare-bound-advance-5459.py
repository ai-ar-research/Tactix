#!/usr/bin/env python3
"""Restore the already-owned SP-FACTOR-1 phase-completion contract in isolation."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
shutil.copytree(root/'.claude/worktrees/runtime-eliminate-limit-5450',
               root/'.claude/worktrees/runtime-bound-advance-5459',symlinks=True,
               ignore=shutil.ignore_patterns('build','.git','__pycache__'))
c=root/'benchmarks/runs/20260907-5459-bound-advance'
c.mkdir(exist_ok=False);(c/'logs').mkdir()
