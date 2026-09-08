#!/usr/bin/env python3
"""Isolate retained-entry eligibility from immutable submitted candidates."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-sweep-cursor-5473',
               root / '.claude/worktrees/runtime-sweep-retained-5510', symlinks=True,
               ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
campaign = root / 'benchmarks/runs/20260908-5510-sweep-retained'
campaign.mkdir(exist_ok=False)
(campaign / 'logs').mkdir()
