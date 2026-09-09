#!/usr/bin/env python3
"""Bind all three parent replays, then inspect both transitive invocations."""
import csv
import itertools
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from transitive_trace import read_phase,reduced_pairs,binary_drops
c=root/'benchmarks/runs/20260909-6410-rank45-transitive-census'
target=c/'summary.json'; assert not target.exists()
arms=('candidate','previous','kissat')
if any(not (c/'results'/f'{a}.json').exists() for a in arms):
    print('Three completed replay records required; no output created'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest
binding=m['parent_summary']; assert sha(root/binding['path'])==binding['sha256']
hashes={str((c/n).relative_to(root)):sha(c/n) for n in ('manifest.json','launch.json')}
phases={}; reduced={}; invocations={}
for arm in arms:
    parent=root/m['parent_runs'][arm]
    p=c/'results'/f'{arm}.json'; q=parent/'results'/p.name
    r=json.loads(p.read_text()); old=json.loads(q.read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==old['binary_sha256']==sha(c/'pins'/arm)
    assert r['partial_certificate']==old['partial_certificate']
    assert set(r['trace_sha256'])-set(old['trace_sha256'])=={arm+'.transitive.tsv'}
    for name,digest in old['trace_sha256'].items(): assert sha(parent/'traces'/name)==digest==r['trace_sha256'][name]
    for name,digest in r['trace_sha256'].items():
        t=c/'traces'/name; assert sha(t)==digest; hashes[str(t.relative_to(root))]=digest
    for t in (p,q): hashes[str(t.relative_to(root))]=sha(t)
    with (c/'traces'/f'{arm}.transitive.tsv').open() as stream:
        phases[arm]=read_phase(stream,'kissat' if arm=='kissat' else 'tactix',76464)
    reduced[arm]=reduced_pairs(phases[arm])
    lines=(c/'traces'/f'{arm}.boundaries.tsv').read_text().splitlines()
    rows=list(csv.DictReader(lines[2:],fieldnames=lines[1][2:].split('\t'),delimiter='\t'))
    invocations[arm]=binary_drops(rows,'transitive' if arm=='kissat' else 'TransitiveReduce',76464)
    assert [r['drop'] for r in invocations[arm]]==([32,0] if arm=='candidate' else [32,5])
    assert not any(r['failed'] for s in phases[arm]['sources'] for r in s['candidates'])
    assert sum(reduced[arm].values())==sum(r['drop'] for r in invocations[arm])
    print(arm,'complete trace/proof replay and framing PASS',flush=True)
def multiset(counter): return [dict(pair=list(p),copies=n) for p,n in sorted(counter.items())]
def first(a,b,fields):
    n=0
    while n<min(len(a),len(b)) and all(a[n][k]==b[n][k] for k in fields): n+=1
    return dict(matching_rows=n,complete_equal=n==len(a)==len(b),fields=fields,left=a[max(0,n-2):n+3],right=b[max(0,n-2):n+3])
pairs={}
for a,b in itertools.combinations(arms,2):
    sources=lambda arm:[s['begin'] for s in phases[arm]['sources']]
    candidates=lambda arm:[r for s in phases[arm]['sources'] for r in s['candidates']]
    pairs[a+'_'+b]=dict(left_only=multiset(reduced[a]-reduced[b]),right_only=multiset(reduced[b]-reduced[a]),
        first_source=first(sources(a),sources(b),['source','candidate_count','out_degree']),
        first_candidate=first(candidates(a),candidates(b),['source','destination','candidate_ordinal','redundant','failed']))
result=dict(rank=45,conflict=76464,parent_replay=True,invocations=invocations,pairs=pairs,
    reduced={a:multiset(r) for a,r in reduced.items()},hashes=hashes,
    limitation='Exact current-own trace/partial-proof replay; all-source framed census localizes missing successful walks. Not graph identity, causal explanation, timing or final-proof acceptance.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({p:{k:v for k,v in x.items() if k in ('left_only','right_only')} for p,x in pairs.items()},indent=2))
