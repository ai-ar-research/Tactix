#!/usr/bin/env python3
"""Fresh6424 observer-only source; never edit a submitted pin or parent."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
authority=root/'benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json'
m=json.loads(authority.read_text()); old=root/m['source']
for rel,digest in m['files'].items(): assert sha(old/rel)==digest,rel
new=root/'.claude/worktrees/runtime-congruence-stages-6508'
c=root/'benchmarks/runs/20260909-6508-congruence-stages-build'
assert not new.exists() and not c.exists()
(c/'logs').mkdir(parents=True)
shutil.copytree(old,new,symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))
(c/'parents.json').write_text(json.dumps(dict(source=str(old.relative_to(root)),candidate=str(new.relative_to(root)),
    files=m['files'],authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),
    purpose='Add read-only actual installed and gates-built binary snapshots to the existing selective Congruence observer. Ordinary Release must stay byte-identical; actual parent1M replay required. No rule, guard, order, budget or proof change.'),indent=2)+'\n')
print(c)
