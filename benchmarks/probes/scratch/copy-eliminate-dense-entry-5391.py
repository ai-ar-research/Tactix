#!/usr/bin/env python3
"""Isolate the coupled Eliminate-entry repair from the submitted publication fix."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-dense-publication-5387',
                root / '.claude/worktrees/runtime-eliminate-dense-entry-5391', symlinks=True,
                ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
c = root / 'benchmarks/runs/20260907-5391-eliminate-dense-entry'
c.mkdir(exist_ok=False); (c / 'logs').mkdir()
