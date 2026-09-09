#!/usr/bin/env python3
"""Clone hash-bound diagnostic sources; never change an existing submitted pin."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
t = root / 'benchmarks/runs/20260908-6127-publication-diagnostic-builds/summary.json'
n = root / 'benchmarks/runs/20260908-6092-rank37-macro-heaps/manifest.json'
tm, nm = [json.loads(p.read_text()) for p in (t, n)]
parent = root / tm['source']
for rel, digest in tm['source_sha256'].items(): assert sha(parent / rel) == digest, rel
for rel, digest in nm['observer_source_hashes'].items(): assert sha(root / rel) == digest, rel
native = root / nm['observer_sources']['kissat']
assert sha(native / 'build/kissat') == nm['hashes']['pins/kissat']
c = root / 'benchmarks/runs/20260909-6206-schedule-continue-build'
assert not c.exists()
parents = {}
for before, after in ((parent, root / '.claude/worktrees/runtime-schedule-continue-6206'),
                      (native, root / 'tools/solvers/variants/kissat-schedule-continue-6206')):
    assert not after.exists()
    shutil.copytree(before, after, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
    parents[str(after.relative_to(root))] = dict(source=str(before.relative_to(root)),
        files={str(p.relative_to(before)): sha(p) for folder in ('src', 'include', 'tests')
               if (before / folder).exists() for p in (before / folder).rglob('*') if p.is_file()})
(c / 'logs').mkdir(parents=True)
(c / 'parents.json').write_text(json.dumps(dict(parents=parents,
    authorities={str(p.relative_to(root)): sha(p) for p in (t, n)}), indent=2) + '\n')
print(c)
