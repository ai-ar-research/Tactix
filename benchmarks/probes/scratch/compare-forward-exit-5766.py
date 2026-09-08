#!/usr/bin/env python3
"""Prove observer neutrality and bind the measured unit-induced early exits."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
c=root / 'benchmarks/runs/20260908-5764-rank31-forward-exit'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''):h.update(block)
    return h.hexdigest()
if (c / 'summary.json').exists():
    print('Already sealed')
    raise SystemExit(0)
p=c / 'results/candidate.json'
if not p.exists():
    print('Pending diagnostic stop')
    raise SystemExit(0)
m=json.loads((c / 'manifest.json').read_text())
r=json.loads(p.read_text())
assert r['exit_code']==0 and r['scratch_deleted']
for rel,digest in m['hashes'].items():assert sha(c / rel)==digest
for rel,digest in r['trace_sha256'].items():assert sha(c / 'traces' / rel)==digest
parent=root / m['parent_campaign']
old=json.loads((parent / 'results/candidate.json').read_text())
assert old['exit_code']==0 and old['scratch_deleted']
for rel,digest in old['trace_sha256'].items():
    assert sha(parent / 'traces' / rel)==digest
    assert r['trace_sha256'][rel]==digest
assert r['partial_certificate']==old['partial_certificate']
extra=set(r['trace_sha256'])-set(old['trace_sha256'])
assert extra=={'candidate.forward-round.tsv'},extra
rounds=[]
for line in (c / 'traces/candidate.forward-round.tsv').read_text().splitlines():
    stage,*fields=line.split()
    values=dict(field.split('=',1) for field in fields)
    if stage=='begin':rounds.append(dict(begin=values))
    else:rounds[-1][stage]=values
assert len(rounds)==2
for row in rounds:
    end=row['end'];stop=row['unit-stop']
    assert end['ended']=='1' and end['interrupted']=='0'
    assert int(end['spent'])<int(end['budget'])
    assert int(end['reached'])==int(stop['index'])+1
    assert int(stop['tail'])+int(end['reached'])==int(end['candidates'])
    row['spent_fraction']=int(end['spent'])/int(end['budget'])
    row['reached_fraction']=int(end['reached'])/int(end['candidates'])
summary=dict(observer_parent_trace_and_proof_equal=True,stop_conflict=m['stop_conflict'],rounds=rounds,
    manifest_sha256=sha(c / 'manifest.json'),result_sha256=sha(p),trace_sha256=r['trace_sha256'],
    conclusion='Rank31 first differing Strengthen179823 ends on a unit with more than99% of its first-round step allowance unused;not a quota cut. Native source continues after dense unit propagation,and actual native/Tactix fixed tail controls establish the omitted mechanism.',
    required_repair='Continue the same forward candidate stream after root consequences,with reference-scoped propagation and connected-watch cleanup;revalidate each later candidate against changed roots,keep chain IDs valid before deleting reasons,and preserve strict quota/interrupt/public action contracts.',
    limitation='No implementation repair or runtime attribution yet. No new Kissat solve. Current5741 B28+C5/full40/fuzz remain separate.')
(c / 'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print('PASS observer-neutral: first pass',rounds[0]['end'],'second pass',rounds[1]['end'])
