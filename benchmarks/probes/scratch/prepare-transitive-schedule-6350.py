#!/usr/bin/env python3
"""Fresh immutable-parent clones for read-only schedule-input instrumentation."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
t=root/'benchmarks/runs/20260909-6305-substitute-collapse-build/manifest.json'
n=root/'benchmarks/runs/20260908-6092-rank37-macro-heaps/manifest.json'
tm,nm=json.loads(t.read_text()),json.loads(n.read_text())
tp,np=root/tm['source'],root/nm['observer_sources']['kissat']
for rel,digest in tm['files'].items():assert sha(tp/rel)==digest
for rel,digest in nm['observer_source_hashes'].items():assert sha(root/rel)==digest
assert sha(np/'build/kissat')==nm['hashes']['pins/kissat']
c=root/'benchmarks/runs/20260909-6350-transitive-schedule-build';assert not c.exists()
(c/'logs').mkdir(parents=True)
parents={}
for before,after,subdir in ((tp,root/'.claude/worktrees/runtime-transitive-schedule-6350','src/policies'),
                           (np,root/'tools/solvers/variants/kissat-transitive-schedule-6350','src')):
    assert not after.exists()
    shutil.copytree(before,after,symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))
    parents[str(after.relative_to(root))]=dict(source=str(before.relative_to(root)),
        files={str(p.relative_to(before)):sha(p) for folder in ('src','include','tests') if (before/folder).exists()
               for p in (before/folder).rglob('*') if p.is_file()})
    shutil.copy2(Path(__file__).with_name('transitive-schedule-6350.h'),after/subdir/'transitive-schedule-6350.h')
(c/'parents.json').write_text(json.dumps(dict(parents=parents,authorities={str(p.relative_to(root)):sha(p) for p in (t,n)},
    purpose='Diagnose observed source936 omission using actual before/after sort inputs and key identities. Read-only instrumentation, not a policy fix or new kernel primitive.'),indent=2)+'\n')
print(c)
