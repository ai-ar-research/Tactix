#!/usr/bin/env python3
"""Isolate the support-count repair without mutating any submitted source."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-sweep-small-5522',
               root / '.claude/worktrees/runtime-factor-root-support-5592',
               symlinks=True, ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260908-5592-factor-root-support'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
