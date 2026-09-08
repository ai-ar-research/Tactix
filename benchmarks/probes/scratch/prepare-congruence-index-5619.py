#!/usr/bin/env python3
"""Isolate the general satisfied-index cleanup; preserve all submitted sources."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-factor-root-support-5592',
               root / '.claude/worktrees/runtime-congruence-index-5619',
               symlinks=True, ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260908-5619-congruence-index'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
