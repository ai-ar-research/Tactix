#!/usr/bin/env python3
"""Freeze the first complete40 solver view and its work-versus-cost evidence."""
from collections import defaultdict
import hashlib
import itertools
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root / 'tools/bench'))
import fixed_runtime
c = root / 'benchmarks/runs/20260908-5811-forward-unit-ranks1-40'
old = root / 'benchmarks/runs/20260908-5758-reuse-target-ranks1-40'
snapshot = c / 'solver-checkpoint.json'
assert not snapshot.exists()
score = fixed_runtime.score(c)
assert score['records'] == score['expected_solves'] == 40
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
reference = fixed_runtime.reference()
def counters(record, native=False):
    out = {}
    for key in ('decisions','conflicts','propagations'):
        expression = r'^c '+key+(r':\s+(\d+)' if native else r'\s+(\d+)')
        matches = [re.match(expression,line) for line in record['solver_statistics']]
        values = [int(match[1]) for match in matches if match]
        assert len(values) == 1,(key,values)
        out[key] = values[0]
    return out
losses = []
for row in score['per_rank']:
    rank = row['rank']
    p = c / 'provisional' / f'{rank-1:03}.json'
    q = old / 'provisional' / p.name
    a,b = json.loads(p.read_text()),json.loads(q.read_text())
    base = next(r for r in reference['records'] if r['rank'] == rank)['measurement']
    assert a['instance'] == b['instance'] == base['instance']
    # Short Lucky cases need not print every native counter.
    native_counts = tactix_counts = None
    if rank >= 27:
        tactix_counts,native_counts = counters(a),counters(base,True)
    stats = lambda r:[line for line in r['solver_statistics'] if not line.startswith('c timing')]
    seconds = a['solve']['wall_seconds']
    ratio = seconds/base['solve']['wall_seconds']
    losses.append(dict(rank=rank,candidate_seconds=seconds,kissat_seconds=base['solve']['wall_seconds'],
        previous_seconds=b['solve']['wall_seconds'],extra_seconds=seconds-base['solve']['wall_seconds'],
        same_previous_certificate=a['certificate_sha256'] == b['certificate_sha256'],
        same_previous_nontiming_statistics=stats(a) == stats(b),
        candidate_counts=tactix_counts,kissat_counts=native_counts,
        descriptive_work_ratios={key:tactix_counts[key]/native_counts[key] for key in tactix_counts} if tactix_counts else None,
        descriptive_seconds_per_work_ratios={key:ratio/(tactix_counts[key]/native_counts[key]) for key in tactix_counts} if tactix_counts else None,
        memory_ratio=a['solve']['peak_rss_kb']/base['solve']['peak_rss_kb'],
        record_sha256=sha(p),previous_record_sha256=sha(q)))
losses.sort(key=lambda row:-row['extra_seconds'])
bc = root / 'benchmarks/runs/20260908-5806-forward-unit-BC-gate'
bm = json.loads((bc / 'manifest.json').read_text())
for rel,digest in bm['pin_sha256'].items(): assert sha(bc / rel) == digest
groups = defaultdict(list)
for p in (bc / 'provisional').glob('*.json'):
    r = json.loads(p.read_text())
    assert r['expected_match'] and not r['solve']['timed_out'] and r['solve']['exit_code'] in (10,20)
    groups[r['arm']].append(r)
assert {arm:len(rs) for arm,rs in groups.items()} == {'candidate_B':28,'current_B':28,'candidate_C':5,'current_C':5}
totals = {arm:sum(r['solve']['wall_seconds'] for r in rs) for arm,rs in groups.items()}
finals = list((bc / 'results').glob('*.json'))
for p in finals:
    r = json.loads(p.read_text())
    assert r['valid'] and not r['cleanup_pending']
bc_view = dict(expected_solves=66,checked_clean=len(finals),totals=totals,
    ratios={tier:totals['candidate_'+tier]/totals['current_'+tier] for tier in ('B','C')},
    combined=sum(totals['candidate_'+tier] for tier in ('B','C'))/sum(totals['current_'+tier] for tier in ('B','C')),
    limitation='Complete solver outcomes,partially checked one-wave regression screen;C must be reported separately,not hidden by combined total.')
snapshot.write_text(json.dumps(score,indent=2)+'\n')
(c / 'next-losses.json').write_text(json.dumps(dict(losses=losses,bc_solver_view=bc_view,
    limitations='Fixed-reference cross-window measurement. Heterogeneous work counters do not isolate instruction/unit cost or prove causal speedups. Parent equivalence is exact proof bytes plus published non-timing statistics,not a hidden whole-state claim.'),indent=2)+'\n')
print(json.dumps(dict(ratio=score['ratio'],seconds=score['candidate_par2'],checked=score['checked_clean'],bc=bc_view,
    top_losses=[{k:row[k] for k in ('rank','candidate_seconds','kissat_seconds','extra_seconds','same_previous_certificate','same_previous_nontiming_statistics','descriptive_work_ratios')} for row in losses[:5]]),indent=2))
