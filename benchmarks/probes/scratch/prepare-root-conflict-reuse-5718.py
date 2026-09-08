#!/usr/bin/env python3
"""Isolate the complete zero-jump conflict-reuse repair from submitted5672."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
shutil.copytree(root/'.claude/worktrees/runtime-assignment-level-5672',
    root/'.claude/worktrees/runtime-root-conflict-reuse-5718',symlinks=True,
    ignore=shutil.ignore_patterns('build','.git','__pycache__'))
c=root/'benchmarks/runs/20260908-5718-root-conflict-reuse'
c.mkdir(exist_ok=False)
(c/'logs').mkdir()
