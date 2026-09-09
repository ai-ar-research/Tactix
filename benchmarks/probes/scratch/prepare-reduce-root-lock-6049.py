#!/usr/bin/env python3
"""Create an isolated5962 successor; no submitted sources are changed."""
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
m=json.loads((root/'benchmarks/runs/20260908-5962-bve-active-pop/manifest.json').read_text())
for rel,digest in m['files'].items(): assert hashlib.sha256((root/m['source']/rel).read_bytes()).hexdigest()==digest,rel
shutil.copytree(root/m['source'],root/'.claude/worktrees/runtime-reduce-root-lock-6049',symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))
(root/'benchmarks/runs/20260908-6049-reduce-root-lock/logs').mkdir(parents=True,exist_ok=False)
