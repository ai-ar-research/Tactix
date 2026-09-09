#!/usr/bin/env python3
"""Clone current source for diagnostic-only nonterminating BVE snapshots."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
a=root/'benchmarks/runs/20260909-6305-substitute-collapse-build/manifest.json'
m=json.loads(a.read_text());parent=root/m['source']
for rel,digest in m['files'].items():assert sha(parent/rel)==digest,rel
c=root/'benchmarks/runs/20260909-6327-collapse-schedule-build'
source=root/'.claude/worktrees/runtime-collapse-schedule-6327'
assert not c.exists() and not source.exists()
shutil.copytree(parent,source,symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))
(c/'logs').mkdir(parents=True)
(c/'parent.json').write_text(json.dumps(dict(source=str(source.relative_to(root)),parent=m['source'],
    authority={'path':str(a.relative_to(root)),'sha256':sha(a)},files=m['files'],
    purpose='Prepare already tested6206 diagnostic continuation on actual6305; no solver semantics change or pending-job mutation.'),indent=2)+'\n')
print(c)
