#!/usr/bin/env python3
"""Isolate an unused-census optimization from all submitted5522 sources."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-sweep-small-5522',
               root / '.claude/worktrees/runtime-vivify-census-5546', symlinks=True,
               ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260908-5546-vivify-census'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
