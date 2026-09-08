#!/usr/bin/env python3
"""Isolate phase-wide BVE exhaustion handling from submitted candidates."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
shutil.copytree(root/'.claude/worktrees/runtime-replacement-cursor-5445',
    root/'.claude/worktrees/runtime-eliminate-limit-5450',symlinks=True,
    ignore=shutil.ignore_patterns('build','.git','__pycache__'))
c=root/'benchmarks/runs/20260907-5450-eliminate-limit'
c.mkdir(exist_ok=False)
(c/'logs').mkdir()
