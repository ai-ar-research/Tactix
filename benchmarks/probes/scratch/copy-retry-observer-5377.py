#!/usr/bin/env python3
"""Create unsubmitted source-only copies; preserve all submitted predecessors."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
for before, after in [('.claude/worktrees/runtime-congruence-prefix-5369', '.claude/worktrees/runtime-retry-producers-5377'),
                      ('tools/solvers/variants/kissat-queue-bumps-5361', 'tools/solvers/variants/kissat-retry-producers-5377')]:
    shutil.copytree(root / before, root / after, symlinks=True,
                    ignore=shutil.ignore_patterns('build', '.git', '__pycache__'))
    print(after)
