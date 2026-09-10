#!/usr/bin/env python3
"""Strict own-pin replay before interpreting overlapping stage wall timers."""
import datetime as dt
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from congruence_timing import parse_cumulative,deltas
c = root/'benchmarks/runs/20260910-6965-xor-stage-time'
target = c/'summary-6967.json'; assert not target.exists()
hashes = {}
def bind(p,expected=None):
    digest = sha(p); assert expected is None or digest == expected,p
    hashes[str(p.relative_to(root))] = digest
m = json.loads((c/'manifest.json').read_text()); bind(c/'manifest.json'); bind(c/'launch.json')
for rel,digest in m['pin_sha256'].items(): bind(c/rel,digest)
for group in ('dependencies','protected_campaign_hashes'):
    for rel,digest in m[group].items(): bind(root/rel,digest)
arms = {}
for task,arm in enumerate(('candidate','previous')):
    p = c/f'results/{task:03d}.json'
    if not p.exists(): p = c/f'provisional/{task:03d}.json'
    r = json.loads(p.read_text()); bind(p)
    state = classify(r,p.parent.name == 'results')
    assert state['status'] in ('pending_validation','checked_UNSAT'),(p,state)
    assert r['task'] == task and r['arm'] == arm and r['rank'] == 47
    binding = m['work_oracles'][arm]['47']; bind(root/binding['path'],binding['sha256'])
    oracle = json.loads((root/binding['path']).read_text())
    for key in ('instance','verdict','certificate_sha256','certificate_bytes'): assert r[key] == oracle[key],(p,key)
    nt = lambda row:[s for s in row['solver_statistics'] if not s.startswith('c timing')]
    assert nt(r) == nt(oracle)
    if p.parent.name == 'results': assert r['binary_sha256'] == m['arms'][arm]['sha256']
    rows = parse_cumulative('\n'.join(r['solver_stderr_tail']),m['expected_timer_rows'])
    arms[arm] = dict(state=state,rows=rows,deltas=deltas(rows),last=rows[-1],wall_seconds=r['solve']['wall_seconds'])
assert [r['items'] for r in arms['candidate']['rows']] == [r['items'] for r in arms['previous']['rows']]
ratios = {key:arms['candidate']['last'][key]/value if value else None for key,value in arms['previous']['last'].items()}
bind(Path(__file__))
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,arms=arms,ratios=ratios,
        exact_complete_work_and_proof=True,limitation='One instrumented pair, not uninstrumented runtime acceptance. Stages overlap and ms are rounded. No native unit-price or global scheduling/CDCL claim. Pending checks remain unverified.'),out,indent=2);out.write('\n')
print(json.dumps(dict(last={a:r['last'] for a,r in arms.items()},ratios=ratios),indent=2))
