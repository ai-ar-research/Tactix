#!/usr/bin/env python3
"""Isolate a behavior-preserving propagation loop from immutable submitted5626."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-congruence-index-repair-5623',
               root / '.claude/worktrees/runtime-propagation-cursor-5641',
               symlinks=True, ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260908-5641-propagation-cursor'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
