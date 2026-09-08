#!/usr/bin/env python3
"""Rank complete fixed-reference losses and compare predecessor work/proofs."""
import hashlib
import itertools
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root / 'benchmarks/runs/20260908-5758-reuse-target-ranks1-40'
current=json.loads((c / 'solver-checkpoint.json').read_text())
old=root / 'benchmarks/runs/20260908-5712-assignment-level-ranks1-40'
previous=json.loads((old / 'solver-checkpoint.json').read_text())
reference=json.loads((root / 'benchmarks/baselines/rank1-40-kissat-12cpu-v1/manifest.json').read_text())
assert current['records']==current['expected_solves']==40
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
losses=[]
for r in current['per_rank']:
    rank=r['rank']
    p=c / 'provisional' / f'{rank-1:03}.json'
    a=json.loads(p.read_text())
    prior=previous['per_rank'][str(rank)]['tactix']
    assert sha(root / prior['record'])==prior['record_sha256']
    b=json.loads((root / prior['record']).read_text())
    base=next(row for row in reference['records'] if row['rank']==rank)
    assert a['instance']==b['instance']==base['instance']
    stats=lambda record:[s for s in record['solver_statistics'] if not s.startswith('c timing')]
    changes=[[x,y] for x,y in itertools.zip_longest(stats(a),stats(b)) if x!=y]
    losses.append(dict(rank=rank,tactix_seconds=r['candidate_par2_seconds'],kissat_seconds=r['kissat_seconds'],
        extra_seconds=r['candidate_par2_seconds']-r['kissat_seconds'],
        parent_seconds=b['solve']['wall_seconds'],same_parent_certificate=a['certificate_sha256']==b['certificate_sha256'],
        parent_statistics_differences=changes,
        memory_ratio=r['candidate_peak_rss_kb']/r['kissat_peak_rss_kb'],
        record_sha256=sha(p)))
losses.sort(key=lambda r:(-r['extra_seconds'],r['rank']))
result=dict(ratio=current['ratio'],candidate_seconds=current['candidate_par2'],
    unchanged_parent_work_and_proof=sum(r['same_parent_certificate'] and not r['parent_statistics_differences'] for r in losses),
    losses=losses,
    limitation='Current fixed-reference ranking,not matched unit-cost attribution. Parent5672 predates root reuse;parent source5718 has no complete40 view. Public-target5741 has exact stopped parent5718 replays,not a full40 predecessor timing control.')
p=c / 'next-losses.json'
assert not p.exists()
p.write_text(json.dumps(result,indent=2)+'\n')
print('Unchanged work/proof vs5672:',result['unchanged_parent_work_and_proof'])
print(json.dumps([{k:r[k] for k in ('rank','tactix_seconds','kissat_seconds','extra_seconds','parent_seconds','same_parent_certificate','memory_ratio')} for r in losses[:6]],indent=2))
