#!/usr/bin/env python3
"""Isolate complete small-environment solving from submitted retained-entry pins."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-sweep-retained-5510',
               root / '.claude/worktrees/runtime-sweep-small-5522', symlinks=True,
               ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
campaign = root / 'benchmarks/runs/20260908-5522-sweep-small'
campaign.mkdir(exist_ok=False)
(campaign / 'logs').mkdir()
