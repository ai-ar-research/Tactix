#!/usr/bin/env python3
"""Isolate the path-specific Vivify retry repair from submitted observer pins."""
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
shutil.copytree(root / '.claude/worktrees/runtime-retry-producers-5377',
                root / '.claude/worktrees/runtime-vivify-retry-5378', symlinks=True,
                ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
