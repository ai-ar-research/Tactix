#!/usr/bin/env python3
"""Require full own neutral replay before interpreting framed Congruence logs."""
from collections import Counter
import itertools
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from congruence_trace import read_binaries,read_candidates
c=root/'benchmarks/runs/20260909-6501-rank33-congruence-replay'
target=c/'summary.json'; assert not target.exists()
if any(not (c/'results'/f'{a}.json').exists() for a in ('candidate','kissat')):
    print('Congruence replay pending; no output created'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest,rel
contract=json.loads((c/'contract-6503.json').read_text())
assert contract['manifest_sha256']==sha(c/'manifest.json')
assert contract['actual_resource_contract']['array']=='0-1'
binding=m['parent_summary']; assert sha(root/binding['path'])==binding['sha256']
hashes={str((c/n).relative_to(root)):sha(c/n) for n in ('manifest.json','launch.json','contract-6503.json')}
binaries={}; candidates={}
for arm,solver in (('candidate','tactix'),('kissat','kissat')):
    parent=root/m['parent_runs'][arm]
    p=c/'results'/f'{arm}.json'; q=parent/'results'/p.name
    r=json.loads(p.read_text()); old=json.loads(q.read_text())
    assert r['arm']==arm and r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==old['binary_sha256']==sha(c/'pins'/arm)
    assert r['partial_certificate']==old['partial_certificate']
    added={f'{arm}.congruence-binaries.tsv',f'{arm}.congruence-candidates.tsv'}
    assert set(r['trace_sha256'])-set(old['trace_sha256'])==added
    for name,digest in old['trace_sha256'].items(): assert sha(parent/'traces'/name)==digest==r['trace_sha256'][name]
    for name,digest in r['trace_sha256'].items():
        t=c/'traces'/name; assert sha(t)==digest; hashes[str(t.relative_to(root))]=digest
    for t in (p,q): hashes[str(t.relative_to(root))]=sha(t)
    with (c/'traces'/f'{arm}.congruence-binaries.tsv').open() as stream:
        binaries[arm]=read_binaries(stream,solver,227617)
    with (c/'traces'/f'{arm}.congruence-candidates.tsv').open() as stream:
        candidates[arm]=read_candidates(stream,solver,227617)
    assert len(binaries[arm])==(6 if arm=='candidate' else 8) and len(candidates[arm])==2
    print(arm,'full own1M trace/partial-proof replay PASS',flush=True)
def difference(a,b):
    x,y=Counter(a),Counter(b)
    return dict(candidate_only=[dict(item=k,count=v) for k,v in sorted((x-y).items())],
                kissat_only=[dict(item=k,count=v) for k,v in sorted((y-x).items())])
comparisons=[]; candidate_comparisons=[]
for call in range(2):
    # Tactix entry is pre-cleanup; native sparse-entry, not dense entry, matches that site.
    # Native exit is extraction exit, whereas Tactix exit is full Congruence exit.
    # Do not align those identically named but semantically different exit records.
    for stage,a_index,b_index in (('incoming-sparse',3*call,4*call),('post-binary-extraction',3*call+1,4*call+2)):
        a,b=binaries['candidate'][a_index],binaries['kissat'][b_index]
        ar,br=[r[:2] for r in a['rows']],[r[:2] for r in b['rows']]
        comparisons.append(dict(call=call,stage=stage,candidate_count=len(ar),kissat_count=len(br),
            equal_multiset=Counter(ar)==Counter(br),equal_external_traversal=ar==br,**difference(ar,br)))
    a,b=candidates['candidate'][call],candidates['kissat'][call]
    first=None
    for index,(x,y) in enumerate(itertools.zip_longest(a['rows'],b['rows'])):
        if x!=y: first=dict(index=index,candidate=x,kissat=y);break
    candidate_comparisons.append(dict(call=call,candidate_count=len(a['rows']),kissat_count=len(b['rows']),
        first_difference=first,key_multiset_difference=difference([r['key'] for r in a['rows']],[r['key'] for r in b['rows']]),
        ends={arm:{k:v for k,v in candidates[arm][call].items() if k!='rows'} for arm in candidates}))
result=dict(rank=33,conflict=227617,parent_replay=True,binary_comparisons=comparisons,
    candidate_comparisons=candidate_comparisons,
    binary_frame_counts={a:[dict(stage=g['stage'],count=g['count'],ticks=g['ticks']) for g in v] for a,v in binaries.items()},
    hashes=hashes,limitation='Sparse entries and post-binary-extraction only are aligned. Native extraction exit is not Tactix full Congruence exit. First census difference does not prove first formula difference, causal repair, final certificate or timing acceptance.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(binary_comparisons=comparisons,candidate_comparisons=candidate_comparisons),indent=2))
