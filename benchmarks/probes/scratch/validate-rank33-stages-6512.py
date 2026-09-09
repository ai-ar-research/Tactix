#!/usr/bin/env python3
"""Neutral own replay before actual installed population and incoming formula comparison."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from congruence_trace import read_binaries
from congruence_stages import read_stages,read_formula
c=root/'benchmarks/runs/20260909-6510-rank33-congruence-stages'
target=c/'summary.json'; assert not target.exists()
if any(not (c/'results'/f'{a}.json').exists() for a in ('candidate','kissat')):
    print('Diagnostic pending; no output created'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
assert m['resource_contract']['array']=='0-1'
hashes={}
def bind(p):
    digest=sha(p); hashes[str(p.relative_to(root))]=digest; return digest
for rel,digest in m['hashes'].items(): assert bind(c/rel)==digest,rel
for key in ('parent_summary','stage_qualification'):
    b=m[key]; assert bind(root/b['path'])==b['sha256']
bind(c/'manifest.json'); bind(c/'launch.json')
binaries={}; formulas={}
for arm,solver in (('candidate','tactix'),('kissat','kissat')):
    parent=root/m['parent_runs'][arm]
    p=c/'results'/f'{arm}.json'; q=parent/'results'/p.name
    r=json.loads(p.read_text()); old=json.loads(q.read_text()); bind(p); bind(q)
    assert r['arm']==arm and r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==sha(c/'pins'/arm)
    assert r['partial_certificate']==old['partial_certificate']
    assert set(r['trace_sha256'])-set(old['trace_sha256'])=={f'{arm}.formula.tsv'}
    for name,digest in old['trace_sha256'].items():
        assert bind(parent/'traces'/name)==digest
        if name==f'{arm}.congruence-binaries.tsv' and arm=='candidate':
            h=hashlib.sha256()
            with (c/'traces'/name).open('rb') as stream:
                for line in stream:
                    fields=line.split(b'\t')
                    if len(fields)>2 and fields[2] in (b'installed',b'gates-built'): continue
                    h.update(line)
            assert h.hexdigest()==digest, 'old binary frames changed'
        else:
            assert r['trace_sha256'][name]==digest, name
    for name,digest in r['trace_sha256'].items(): assert bind(c/'traces'/name)==digest,name
    with (c/'traces'/f'{arm}.congruence-binaries.tsv').open() as stream:
        binaries[arm]=read_stages(stream,227617) if arm=='candidate' else read_binaries(stream,solver,227617)
    assert len(binaries[arm])==(10 if arm=='candidate' else 8)
    with (c/'traces'/f'{arm}.formula.tsv').open() as stream:
        formulas[arm]=list(read_formula(stream,solver))
    assert formulas[arm], 'missing selected formula'
    print(arm,'full own1M traces/partial-proof and old census replay PASS',flush=True)
def diff(a,b):
    x,y=Counter(a),Counter(b); left,right=x-y,y-x
    return dict(equal_multiset=x==y,candidate_count=sum(x.values()),kissat_count=sum(y.values()),
        candidate_only_count=sum(left.values()),kissat_only_count=sum(right.values()),
        candidate_only_examples=list(sorted(left.items()))[:20],kissat_only_examples=list(sorted(right.items()))[:20])
comparisons=[]
for call in range(2):
    for stage,ai,bi in (('incoming-sparse',0,0),('clean-preinstall',1,1),('actually-installed',2,2)):
        a,b=binaries['candidate'][5*call+ai],binaries['kissat'][4*call+bi]
        comparisons.append(dict(call=call,stage=stage,**diff([r[:2] for r in a['rows']],[r[:2] for r in b['rows']])))
    # Descriptive only: native also merges matching gate outputs during extraction.
    a,b=binaries['candidate'][5*call+3],binaries['kissat'][4*call+3]
    comparisons.append(dict(call=call,stage='post-gate-observations-NOT-equivalent-work',
        **diff([r[:2] for r in a['rows']],[r[:2] for r in b['rows']])) )
formula=dict(identities=diff([r[0] for r in formulas['candidate']],[r[0] for r in formulas['kissat']]),
    identities_and_values=diff([(r[0],r[2]) for r in formulas['candidate']],[(r[0],r[2]) for r in formulas['kissat']]),
    identities_and_pose=diff([(r[0],r[1]) for r in formulas['candidate']],[(r[0],r[1]) for r in formulas['kissat']]),
    counts={arm:dict(Counter('red' if r[0][0] else 'irred' for r in rows)) for arm,rows in formulas.items()})
result=dict(rank=33,conflict=227617,parent_replay=True,binary_comparisons=comparisons,first_call_large_input=formula,
    binary_frame_counts={arm:[{k:v for k,v in g.items() if k!='rows'} for g in groups] for arm,groups in binaries.items()},
    hashes=hashes,limitation='Actual installed comparison is aligned. Gates-built/native extraction exit observations include different work: native new gates can merge outputs during extraction; not a missing inference proof. Incoming formula multiset equality does not imply equal watch state. Bounded partial proofs are not final checked certificates; no timing claim.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(binary_comparisons=comparisons,first_call_large_input=formula),indent=2))
