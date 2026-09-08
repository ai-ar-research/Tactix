#!/usr/bin/env python3
"""Separate diagnostic sources for one selected Factor lifecycle window."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
for before,after in [
 ('.claude/worktrees/runtime-eliminate-limit-5450','.claude/worktrees/runtime-factor-observer-5456'),
 ('tools/solvers/variants/kissat-imply-fast-5414','tools/solvers/variants/kissat-factor-observer-5456')]:
    shutil.copytree(root/before,root/after,symlinks=True,
                   ignore=shutil.ignore_patterns('build','.git','__pycache__'))
c=root/'benchmarks/runs/20260907-5456-factor-observer'
c.mkdir(exist_ok=False)
(c/'logs').mkdir()
