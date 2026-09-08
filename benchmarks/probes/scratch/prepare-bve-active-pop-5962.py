#!/usr/bin/env python3
"""Independent policy repair source from retained5900, without observers."""
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
m=json.loads((root/'benchmarks/runs/20260908-5900-reduce-root/manifest.json').read_text())
for rel,digest in m['files'].items():assert hashlib.sha256((root/m['source']/rel).read_bytes()).hexdigest()==digest
shutil.copytree(root/m['source'],root/'.claude/worktrees/runtime-bve-active-pop-5962',symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))
(root/'benchmarks/runs/20260908-5962-bve-active-pop/logs').mkdir(parents=True,exist_ok=False)
