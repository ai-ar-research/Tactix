#!/usr/bin/env python3
"""Read published owned records only; fail on any invalidity before scoring."""
from collections import Counter
import json, sys
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha
for name,count in (('6584-material-schedule-BC-gate',66),
                   ('6592-rank50-material-three-wave-runtime',6),
                   ('6594-material-ranks1-50',50)):
    c=root/'benchmarks/runs'/('20260909-'+name)
    m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['pin_sha256'].items(): assert sha(c/rel)==digest,rel
    records={}; paths={}; states={}
    for folder in ('provisional','results'):
        for path in sorted((c/folder).glob('*.json')):
            r=json.loads(path.read_text()); task=r['task']
            assert int(path.stem)==task and 0<=task<count
            state=classify(r,folder=='results')
            # A solver timeout is a scored performance loss, not an invalidity.
            # Keep it visible, pending final cleanup; abnormal exits still fail.
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT',
                                      'solver_timeout','pending_cleanup_solver_timeout'),(path,state)
            if folder=='results': assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
            if task in records:
                for key in ('solve','certificate_sha256','certificate_bytes'): assert r[key]==records[task][key]
            records[task]=r; paths[task]=path; states[task]=state
    print(name, len(records),'/',count,dict(Counter(s['status'] for s in states.values())))
    if 'three-wave' in name:
        print([(t,r['arm'],r['solve']['wall_seconds']) for t,r in sorted(records.items())])
    if 'ranks1-50' in name:
        print('missing ranks:',[i+1 for i in range(count) if i not in records])
