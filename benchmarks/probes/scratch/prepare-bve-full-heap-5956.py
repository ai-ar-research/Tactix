#!/usr/bin/env python3
"""Create diagnostic-only clones; never edit a submitted source or pin."""
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
m=json.loads((root/'benchmarks/runs/20260908-5900-reduce-root/manifest.json').read_text())
for rel,digest in m['files'].items():assert hashlib.sha256((root/m['source']/rel).read_bytes()).hexdigest()==digest
n=json.loads((root/'benchmarks/runs/20260908-5949-bve-heap-lifecycle/manifest.json').read_text())
for rel,digest in n['native_source_files'].items():assert hashlib.sha256((root/n['native_source']/rel).read_bytes()).hexdigest()==digest
for before,after in [(m['source'],'.claude/worktrees/runtime-bve-full-heap-5956'),
                     (n['native_source'],'tools/solvers/variants/kissat-bve-full-heap-5956')]:
    shutil.copytree(root/before,root/after,symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))
c=root/'benchmarks/runs/20260908-5956-bve-full-heap-controls'
(c/'logs').mkdir(parents=True,exist_ok=False)
print(c)
