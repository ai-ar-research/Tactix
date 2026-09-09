#!/usr/bin/env python3
"""Clone only the frozen native diagnostic source for a read-only removal observer."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260909-6206-schedule-continue-build/summary.json'
s = json.loads(authority.read_text())
rel = 'tools/solvers/variants/kissat-schedule-continue-6206'
parent = root / rel
for p, digest in s['sources'][rel].items(): assert sha(parent / p) == digest
new = root / 'tools/solvers/variants/kissat-removal-clause-6262'
c = root / 'benchmarks/runs/20260909-6262-removal-observer-build'
assert not new.exists() and not c.exists()
shutil.copytree(parent, new, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
(c / 'parents.json').write_text(json.dumps(dict(parent=rel, source=str(new.relative_to(root)),
    parent_files=s['sources'][rel], authority=dict(path=str(authority.relative_to(root)), sha256=sha(authority))), indent=2)+'\n')
print(new)
