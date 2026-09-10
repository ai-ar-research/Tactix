#!/usr/bin/env python3
"""Distinguish changed bookkeeping counters from changed search or certificates."""
import itertools
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7034-collect-retry-BC-gate'
target = c / 'work-classification-7043.json'
assert not target.exists()
p = c / 'summary-7036.json'
g = json.loads(p.read_text())
assert g['solver_records'] == 66
hashes = dict(g['hashes'])
for rel, digest in hashes.items(): assert sha(root / rel) == digest, rel
for path in (p, Path(__file__)): hashes[str(path.relative_to(root))] = sha(path)
records = {}
for rel in g['hashes']:
    path = root / rel
    if path.parent.parent == c and path.parent.name in ('results', 'provisional'):
        r = json.loads(path.read_text()); records[r['task']] = r
assert set(records) == set(range(66))
rows = []
for task in range(0, 66, 2):
    a, b = records[task], records[task+1]
    left = [s for s in a['solver_statistics'] if not s.startswith('c timing')]
    right = [s for s in b['solver_statistics'] if not s.startswith('c timing')]
    changes = [dict(current=x, candidate=y) for x, y in itertools.zip_longest(left, right) if x != y]
    bookkeeping_only = bool(changes) and all(x['current'] and x['candidate'] and
        x['current'].startswith('c touch ') and x['candidate'].startswith('c touch ') for x in changes)
    rows.append(dict(instance=a['instance'], rank=a['rank'], differences=changes,
        only_touch_line_changed=bookkeeping_only,
        certificate_equal=a['certificate_sha256'] == b['certificate_sha256'] and a['certificate_bytes'] == b['certificate_bytes']))
out = dict(hashes=hashes, pairs=rows,
    unchanged=sum(not r['differences'] for r in rows),
    touch_only=sum(r['only_touch_line_changed'] for r in rows),
    other_changes=sum(bool(r['differences']) and not r['only_touch_line_changed'] for r in rows),
    exact_certificates=sum(r['certificate_equal'] for r in rows),
    limitation='Exact reported-line comparison, not hidden internal-state equality. Do not call every non-timing counter difference changed CDCL search work; preserve complete differing lines. Correctness/check states remain owned by7036 and successor audits.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps({key: out[key] for key in ('unchanged', 'touch_only', 'other_changes', 'exact_certificates')}, indent=2))
