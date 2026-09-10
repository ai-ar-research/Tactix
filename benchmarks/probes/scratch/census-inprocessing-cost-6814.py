#!/usr/bin/env python3
"""Select current inprocessing cost controls, retaining all50 and parity gaps."""
from collections import Counter
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from runtime_summary import counters
from inprocessing_cost import read_timers
c=root/'benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50'
target=root/'benchmarks/runs/20260910-6814-inprocessing-cost-census/summary.json'
assert not target.exists()
baseline=root/'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json'
native=json.loads(baseline.read_text());m=json.loads((c/'manifest.json').read_text())
hashes={str(baseline.relative_to(root)):sha(baseline),str((c/'manifest.json').relative_to(root)):sha(c/'manifest.json')}
for rel,digest in m['pin_sha256'].items():assert sha(c/rel)==digest,rel
rows=[];totals=Counter();checked=0
for task in range(50):
    p=c/f'results/{task:03d}.json'
    if not p.exists():p=c/f'provisional/{task:03d}.json'
    r=json.loads(p.read_text());state=classify(r,p.parent.name=='results')
    assert state['status'] in ('pending_validation','checked_UNSAT','checked_SAT'),(p,state)
    assert r['rank']==task+1 and r['arm']=='tactix'
    checked+=bool(state['checked'] and state['clean'])
    hashes[str(p.relative_to(root))]=sha(p)
    nr=next(x for x in native['records'] if x['rank']==r['rank'])
    assert r['instance']==nr['instance'] and r['expected']==nr['expected']
    timers=read_timers(r['solver_statistics'])
    for rule,timing in timers['rules'].items():totals[rule]+=timing['ms']
    current_work,reference_work=counters(r),counters(nr['measurement'])
    rows.append(dict(rank=r['rank'],expected=r['expected'],state=state,timers=timers,
        solver_seconds=r['solve']['wall_seconds'],reference_seconds=nr['measurement']['solve']['wall_seconds'],
        current_work=current_work,reference_work=reference_work,
        conflicts_decisions_equal=all(current_work[k]==reference_work[k] for k in ('conflicts','decisions'))))
assert [r['rank'] for r in rows]==list(range(1,51))
assert rows[31]['conflicts_decisions_equal']
for p in (Path(__file__),root/'tools/bench/inprocessing_cost.py',root/'tools/bench/test_inprocessing_cost.py',
    root/'tools/solvers/variants/kissat-macro-heap-6090/src/profile.c',
    root/'.claude/worktrees/runtime-reduce-watermark-6730/src/main.cpp'):
    hashes[str(p.relative_to(root))]=sha(p)
out=dict(hashes=hashes,rows=rows,checked_clean_at_snapshot=checked,
    rule_totals_ms=dict(totals.most_common()),total_slot_ms=sum(r['timers']['slot_ms'] for r in rows),
    selected_ranks=[32,39],
    selection='Rank32 UNSAT preserves exact full native conflicts/decisions and is a broad inprocessing-cost control. Rank39 UNSAT is Sweep-heavy and exposes a different mix; its work is not equal to native. Vivify has the largest aggregate named-rule timer, followed by Sweep,Congruence,BVE. Profile current Release before selecting a general implementation change.',
    parity='Preserve exact candidate own-work/proof replay for cost-only changes; audit inprocessing admission/limits and CDCL state separately. Full-count equality on32 is not universal CDCL parity:31/35 and21 still have substantial differing-work gaps; stronger33 is retained. Propagation counters and method boundaries differ between solvers.',
    limitation='Timers are attribution, not exact unit prices; counts of rule applications are not equal logical operations. Native profile.c accumulates nested intervals, so do not subtract or sum differently scoped native/Tactix timers. Fixed7239.010s denominator unchanged. Pending checks allow work, not acceptance.',
    test_consideration='Five new timer tests cover exact integer fields, duplicate/missing/malformed rows, partition-rounding and zero-slot cases. No solver behavior changes; all50 records retained.')
target.parent.mkdir()
with target.open('x') as stream:json.dump(out,stream,indent=2);stream.write('\n')
print('named rule seconds',[(k,v/1000) for k,v in totals.most_common()])
print('all50 slot seconds',out['total_slot_ms']/1000,'checked',checked)
