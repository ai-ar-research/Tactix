#!/usr/bin/env python3
"""Clone only the validated native admission observer; keep old builds frozen."""
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260909-6283-admission-observer-build/manifest.json'
m = json.loads(authority.read_text())
parent_rel = 'tools/solvers/variants/kissat-admission-observer-6283'
parent = root / parent_rel
for rel, digest in m['files'][parent_rel].items(): assert sha(parent / rel) == digest, rel
assert sha(parent / 'build/kissat') == m['binaries'][parent_rel + '/build/kissat']
new = root / 'tools/solvers/variants/kissat-material-observer-6550'
c = root / 'benchmarks/runs/20260909-6550-native-material-observer-build'
assert not c.exists() and not new.exists()
(c / 'logs').mkdir(parents=True)
shutil.copytree(parent, new, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
with (c / 'parents.json').open('x') as out:
    json.dump(dict(source=str(new.relative_to(root)), parent=parent_rel,
        files=m['files'][parent_rel], authority=dict(path=str(authority.relative_to(root)), sha256=sha(authority))), out, indent=2)
    out.write('\n')
print(new)
