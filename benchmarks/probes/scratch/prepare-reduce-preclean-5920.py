#!/usr/bin/env python3
"""Compose only the two independently tested repairs in a fresh source tree."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
for name in ('20260908-5900-reduce-root', '20260908-5870-forward-preclean'):
    m = json.loads((root / 'benchmarks/runs' / name / 'manifest.json').read_text())
    for rel, digest in m['files'].items():
        assert hashlib.sha256((root / m['source'] / rel).read_bytes()).hexdigest() == digest
source = root / '.claude/worktrees/runtime-reduce-preclean-5920'
campaign = root / 'benchmarks/runs/20260908-5920-reduce-preclean'
assert not source.exists() and not campaign.exists()
shutil.copytree(root / '.claude/worktrees/runtime-reduce-root-5900', source,
                symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(campaign / 'logs').mkdir(parents=True)
print(source)
