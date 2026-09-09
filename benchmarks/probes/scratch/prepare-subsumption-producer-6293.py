#!/usr/bin/env python3
"""Clone the fully replayed native observer to locate actual material producers."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
authority = root/'benchmarks/runs/20260909-6283-admission-observer-build/manifest.json'
m = json.loads(authority.read_text())
parent = root/'tools/solvers/variants/kissat-admission-observer-6283'
files = m['files'][str(parent.relative_to(root))]
for rel, digest in files.items(): assert sha(parent/rel) == digest, rel
target = root/'tools/solvers/variants/kissat-subsumption-producer-6293'
c = root/'benchmarks/runs/20260909-6293-subsumption-producer-build'
assert not target.exists() and not c.exists()
(c/'logs').mkdir(parents=True)
shutil.copytree(parent, target, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c/'parents.json').write_text(json.dumps(dict(source=str(target.relative_to(root)), parent=str(parent.relative_to(root)), files=files,
    authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority))),indent=2)+'\n')
print(target)
