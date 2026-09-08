#!/usr/bin/env python3
"""Normalize Factor member ordinals without confusing them with emission order."""
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5457-rank37-factor-window'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''):h.update(block)
    return h.hexdigest()
calls={}
for arm in ('candidate','kissat'):
    result=json.loads((c/'results'/f'{arm}.json').read_text())
    path=c/'traces'/f'{arm}.events.tsv'
    assert sha(path)==result['trace_sha256'][path.name]
    output=[];call=None;grid=None
    with path.open() as f:
        for line in f:
            if line.startswith('#') or not line.strip():continue
            row=line.rstrip('\n').split('\t');event=row[2]
            p=dict(v.split('=',1) for v in row[-1].split('|') if '=' in v)
            if event=='factor_schedule' and p['ordinal']=='0':
                call=dict(seeds=[],grids=[]);output.append(call)
            if event=='factor_seed':call['seeds'].append(p)
            if event=='factor_grid':
                grid=dict(dimensions={k:p[k] for k in ('factors','remainders','delta')},members={})
                call['grids'].append(grid)
            if event=='factor_grid_member':
                ordinal=int(p['ordinal']);assert ordinal not in grid['members']
                grid['members'][ordinal]=p.get('external',p.get('lit'))
    assert len(output)==2
    calls[arm]=output
t,k=calls['candidate'],calls['kissat']
assert t[0]['grids']==k[0]['grids']
assert t[1]['seeds'][0]['external']==k[1]['seeds'][0]['external']=='-145'
assert t[1]['grids'][0]['dimensions']['delta']=='2'
assert k[1]['grids'][0]['dimensions']['delta']=='55'
out=dict(first_call_normalized_grids_equal=True,calls=calls,
    conclusion='Both initial schedules and scores match (summary.json). First-call grid members match by ordinal; raw emission order is intentionally reversed in native logging. Second call begins at seed -145 in both; Tactix accepts gain 2, native proceeds to seed -488187 and accepts gain 55.',
    source_hypothesis='5450 stalls bound 1 on the preceding BVE quota cut. Native consistent-phase bookkeeping advances to 2, and Factor requires gain strictly above that bound. Candidate 5459 restores this policy; 5458 verbose confirmation and 5463 causal prefix remain independent checks.',
    limitation='No full runtime gain inferred from a trace or source hypothesis.')
p=c/'bound-localization.json';assert not p.exists();p.write_text(json.dumps(out,indent=2)+'\n')
print('PASS: first-call ordinal-normalized grids equal; second-call gain-2 acceptance localized')
