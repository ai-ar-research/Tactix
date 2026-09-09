#!/usr/bin/env python3
"""Clone the stamp repair for the existing read-only scheduler observer."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
authority=root/'benchmarks/runs/20260909-6357-compact-stamp-build/manifest.json'
m=json.loads(authority.read_text()); old=root/m['source']
for rel,digest in m['files'].items(): assert sha(old/rel)==digest
new=root/'.claude/worktrees/runtime-rank45-schedule-6413'
c=root/'benchmarks/runs/20260909-6413-rank45-schedule-build'
assert not new.exists() and not c.exists()
(c/'logs').mkdir(parents=True)
shutil.copytree(old,new,symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))
shutil.copy2(Path(__file__).with_name('transitive-schedule-6350.h'),new/'src/policies/transitive-schedule-6350.h')
(c/'parents.json').write_text(json.dumps(dict(source=str(old.relative_to(root)),candidate=str(new.relative_to(root)),
    files=m['files'],authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),
    purpose='Read-only before/after schedule keys at Rank45 conflict76464; preserve all6410 traces and each own partial proof. No comparator, policy, API, guard or proof change.'),indent=2)+'\n')
print(c)
