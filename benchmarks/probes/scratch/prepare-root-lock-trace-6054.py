#!/usr/bin/env python3
"""Separate diagnostic clone: reflect root-lock semantics in trace annotations."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
shutil.copytree(root/'.claude/worktrees/runtime-reduce-root-lock-6049',root/'.claude/worktrees/runtime-reduce-root-lock-trace-6054',symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))
(root/'benchmarks/runs/20260908-6054-root-lock-trace-controls/logs').mkdir(parents=True,exist_ok=False)
