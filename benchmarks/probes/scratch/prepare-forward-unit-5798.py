#!/usr/bin/env python3
"""Isolate the complete forward-unit continuation repair from submitted5741."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
authority = root / 'benchmarks/runs/20260908-5741-reuse-target/manifest.json'
m = json.loads(authority.read_text())
source = root / m['source']
target = root / '.claude/worktrees/runtime-forward-unit-5798'
campaign = root / 'benchmarks/runs/20260908-5798-forward-unit'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['files'].items(): assert sha(source / rel) == digest, rel
assert not target.exists() and not campaign.exists()
shutil.copytree(source, target, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
campaign.mkdir()
(campaign / 'logs').mkdir()
(campaign / 'preparation.json').write_text(json.dumps(dict(
    parent_authority=str(authority.relative_to(root)), parent_sha256=sha(authority),
    source=str(target.relative_to(root)), production=m['production'],
    parent_strengthen_sha256=sha(source / 'src/rules/strengthen/strengthen.cpp'),
    parent_header_sha256=sha(source / 'src/rules/strengthen/strengthen.hpp'),
    purpose='Complete connected forward-unit propagation and continuation, preserving N1/N2 and scheduling currency. No submitted source modified.'), indent=2) + '\n')
print(target)
