#!/usr/bin/env python3
"""Create an isolated successor; never edit a submitted source or pin."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
authority = root / 'benchmarks/runs/20260908-5830-forward-admission/manifest.json'
m = json.loads(authority.read_text())
parent = root / m['source']
for rel, digest in m['files'].items():
    assert hashlib.sha256((parent / rel).read_bytes()).hexdigest() == digest
source = root / '.claude/worktrees/runtime-forward-caller-5849'
campaign = root / 'benchmarks/runs/20260908-5849-forward-caller'
assert not source.exists() and not campaign.exists()
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(campaign / 'logs').mkdir(parents=True)
print(source)
