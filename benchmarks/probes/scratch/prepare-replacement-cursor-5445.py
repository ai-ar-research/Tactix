#!/usr/bin/env python3
"""Isolate wraparound membership search without modifying submitted sources."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-vivify-max-5438',
               root / '.claude/worktrees/runtime-replacement-cursor-5445',
               symlinks=True, ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
campaign = root / 'benchmarks/runs/20260907-5445-replacement-cursor'
campaign.mkdir(exist_ok=False)
(campaign / 'logs').mkdir()
