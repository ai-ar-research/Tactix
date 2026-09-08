#!/usr/bin/env python3
"""Create independent trace-only sources; never modify a submitted source."""
import hashlib
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
assert hashlib.sha256((root/'tools/solvers/variants/kissat-imply-fast-5414/build/kissat').read_bytes()).hexdigest()=='849db948ead9166917d9598e02cea6b34703a0f87c7bc5b437a704b24099dc16'
for before,after in [
    ('.claude/worktrees/runtime-propagation-cursor-repair-5647','.claude/worktrees/runtime-score-bump-observer-5696'),
    ('tools/solvers/variants/kissat-imply-fast-5414','tools/solvers/variants/kissat-score-bump-observer-5696')]:
    shutil.copytree(root/before,root/after,symlinks=True,
        ignore=shutil.ignore_patterns('build','.git','__pycache__'))
c=root/'benchmarks/runs/20260908-5696-score-bump-observer'
c.mkdir(exist_ok=False);(c/'logs').mkdir()
