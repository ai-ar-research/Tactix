#!/usr/bin/env python3
"""Create an isolated source-only successor; never modify submitted sources."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-vivify-retry-5378'
target = root / '.claude/worktrees/runtime-dense-publication-5387'
shutil.copytree(source, target, symlinks=True,
                ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260907-5387-dense-publication'
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
print(target)
