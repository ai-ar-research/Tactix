#!/usr/bin/env python3
"""Fail-closed campaign checkpoint, keeping pending checks provisional."""
import json
import math
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha,score
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-09-substitute-retry-checkpoint-6247.json'
assert not target.exists()
campaigns={}; hashes={}
for name in ('20260909-6236-substitute-retry-BC-gate','20260909-6239-substitute-retry-ranks1-50','20260909-6181-learned-sort-ranks1-50'):
    c=root/'benchmarks/runs'/name; m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['pin_sha256'].items(): assert sha(c/rel)==digest
    records={}; states={}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r=json.loads(p.read_text()); state=classify(r,folder=='results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
            records[r['task']]=r; states[r['task']]=state
            hashes[str(p.relative_to(root))]=sha(p)
    data=dict(solver_records=len(records),checked=sum(s['checked'] for s in states.values()),states=states,
              launch=json.loads((c/'launch.json').read_text()),manifest_sha256=sha(c/'manifest.json'))
    if 'BC-gate' in name and len(records)==66:
        totals={}
        for r in records.values(): totals.setdefault(r['arm'],[]).append(r['solve']['wall_seconds'])
        data['totals']={a:math.fsum(v) for a,v in totals.items()}
        data['candidate_over_production']={t:data['totals']['candidate_'+t]/data['totals']['current_'+t] for t in ('B','C')}
    if 'ranks1-50' in name and len(records)==50: data['score']=score(c)
    campaigns[name]=data
target.write_text(json.dumps(dict(campaigns=campaigns,hashes=hashes,limitation='Pending certificates permit development only. Fixed reference is cross-window, not causal repair attribution or promotion.'),indent=2)+'\n')
print(json.dumps({n:{k:v for k,v in d.items() if k in ('solver_records','checked','candidate_over_production')} for n,d in campaigns.items()},indent=2))
