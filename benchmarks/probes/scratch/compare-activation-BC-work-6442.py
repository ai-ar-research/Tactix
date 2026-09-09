#!/usr/bin/env python3
"""Bind all33 current/parent candidate outcomes; retain every changed-work case."""
import csv
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from runtime_summary import counters
c=root/'benchmarks/runs/20260909-6427-activation-stamp-BC-gate'
p=root/'benchmarks/runs/20260909-6387-vivify-count-cache-BC-gate'
target=c/'parent-work-checkpoint-6442.json'; assert not target.exists()
gate=c/'solver-checkpoint-6429.json'; g=json.loads(gate.read_text()); assert g['solver_records']==66
hashes={str(gate.relative_to(root)):sha(gate)}
for run in (c,p):
    m=json.loads((run/'manifest.json').read_text()); hashes[str((run/'manifest.json').relative_to(root))]=sha(run/'manifest.json')
    for rel,digest in m['pin_sha256'].items(): assert sha(run/rel)==digest
def read(run,task):
    q=run/'results'/f'{task:03}.json'
    if not q.exists(): q=run/'provisional'/q.name
    r=json.loads(q.read_text()); state=classify(r,q.parent.name=='results')
    assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(q,state)
    hashes[str(q.relative_to(root))]=sha(q); return r,state['status']
with (c/'matrix.tsv').open() as stream: matrix=list(csv.DictReader(stream,delimiter='\t'))
rows=[]
for row in matrix:
    if not row['arm'].startswith('candidate'): continue
    task=int(row['task']); current,state=read(c,task); parent,old_state=read(p,task)
    for key in ('arm','instance','expected','verdict'): assert current[key]==parent[key]
    assert current['instance']==row['instance']
    nt=lambda r:[line for line in r['solver_statistics'] if not line.startswith('c timing')]
    rows.append(dict(task=task,tier=row['tier'],instance=row['instance'],expected=row['expected'],
        validation=state,parent_validation=old_state,
        proof_exact=current['certificate_sha256']==parent['certificate_sha256'] and current['certificate_bytes']==parent['certificate_bytes'],
        work_exact=nt(current)==nt(parent),current=counters(current),parent=counters(parent),
        current_seconds=current['solve']['wall_seconds'],parent_seconds=parent['solve']['wall_seconds']))
assert len(rows)==33
result=dict(candidate_pairs=33,rows=rows,hashes=hashes,
    changed_work_tasks=[r['task'] for r in rows if not r['work_exact']],
    changed_proof_tasks=[r['task'] for r in rows if not r['proof_exact']],
    test_consideration='Existing result-classification/counter tests reused. Require all33 identical input/arm/verdict identities and retain all non-timing statistics in equality; no parser change.',
    limitation='Cross-window guard decomposition, not isolated runtime gain. Exact work/proof does not establish whole graph or ordering-state identity.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('rows','hashes')},indent=2))
for row in rows:
    if not row['work_exact']: print(json.dumps(row))
