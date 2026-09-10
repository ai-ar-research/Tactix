#!/usr/bin/env python3
"""Price the two independent, complete same-work SubSolver experiments."""
from pathlib import Path
import datetime as dt
import json
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from inprocessing_cost import paired_costs
assert len(sys.argv) == 2 and sys.argv[1] in ('shrink','binary')
stem,number = {
    'shrink':('20260910-6864-subsolver-shrink-runtime',6866),
    'binary':('20260910-6882-subsolver-binary-runtime',6884),
}[sys.argv[1]]
c = root/'benchmarks/runs'/stem
target = c/'timer-cost-6887.json'
assert not target.exists(),'successful writer frozen'
checkpoint = c/f'solver-checkpoint-{number}.json'
s = json.loads(checkpoint.read_text())
assert s['solver_outcomes'] == 18 and s['exact_per_arm_work_and_proof']
for rel,digest in s['hashes'].items(): assert sha(root/rel) == digest,rel
rows = {}
for rel in s['hashes']:
    p = root/rel
    if p.parent.name not in ('provisional','results'): continue
    row = json.loads(p.read_text())
    if row['task'] in rows and p.parent.name == 'provisional': continue
    rows[row['task']] = row
assert set(rows) == set(range(18))
out = paired_costs(list(rows.values()),(32,39,38),3,('Sweep','BVE','Vivify'))
for rank,r in out.items():
    old = s['ranks'][str(rank)]['paired']
    assert all(r['paired']['wall_seconds'][key] == old[key] for key in ('median','min','max'))
paths = [Path(__file__),checkpoint,root/'tools/bench/inprocessing_cost.py',root/'tools/bench/test_inprocessing_cost.py']
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),experiment=sys.argv[1],
    hashes={str(p.relative_to(root)):sha(p) for p in paths},ranks=out,
    solved=dict(candidate=9,previous=9,SAT=0,UNSAT=18,timeouts=0,solve_losses=0),
    paired_par2_seconds={arm:sum(r['solve']['wall_seconds'] for r in rows.values() if r['arm']==arm)
                        for arm in ('candidate','previous')},
    test_consideration='Eight new maintained paired-matrix tests cover exact reversed-order ratios, complete cardinality, wrong/duplicate pairs, application changes, missing rules, zero denominator, invalid numeric costs and declarations; five timer-parser tests retained. Upstream strict checkpoint requires exact complete work/certificates. This helper independently matches every runtime ratio.',
    limitation='Same-work current6730 comparison on three selected ranks, not native work equality or fixed50 competitiveness. Pending proofs remain pending. RSS is process peak, not watch-only memory; differing allocator/code layout may affect it. Price all waves and adverse controls; no best-of repeats or composition. Source fuzz, full50 and memory acceptance remain open.')
with target.open('x') as stream:
    json.dump(result,stream,indent=2);stream.write('\n')
print(json.dumps({rank:r['paired'] for rank,r in out.items()},indent=2))
