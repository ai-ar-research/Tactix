#!/usr/bin/env python3
"""Require full unchanged-pin parent replay before interpreting census differences."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from transitive_trace import read_phase,reduced_pairs
c=root/'benchmarks/runs/20260909-6340-rank32-transitive-census'
assert not (c/'summary.json').exists()
subprocess.run(['python3','-m','unittest','discover','-s','tools/bench','-p','test_transitive_trace.py'],cwd=root,check=True)
if not all((c/'results'/f'{arm}.json').exists() for arm in ('candidate','kissat')):
    print('Both completed records required; no partial interpretation.');sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(c/rel)==digest
binding=m['parent_summary'];assert sha(root/binding['path'])==binding['sha256']
hashes={};phases={}
for name in ('manifest.json','launch.json'):hashes[str((c/name).relative_to(root))]=sha(c/name)
for arm in ('candidate','kissat'):
    parent=root/m['parent_runs'][arm]
    q=c/'results'/f'{arm}.json';r=json.loads(q.read_text())
    p=parent/'results'/q.name;before=json.loads(p.read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['arm']==arm and r['binary_sha256']==before['binary_sha256']==sha(c/'pins'/arm)
    assert r['partial_certificate']==before['partial_certificate']
    assert set(r['trace_sha256'])-set(before['trace_sha256'])=={arm+'.transitive.tsv'}
    for name,digest in before['trace_sha256'].items():
        assert sha(parent/'traces'/name)==digest==r['trace_sha256'][name],name
    for name,digest in r['trace_sha256'].items():
        path=c/'traces'/name;assert sha(path)==digest;hashes[str(path.relative_to(root))]=digest
    hashes[str(p.relative_to(root))]=sha(p);hashes[str(q.relative_to(root))]=sha(q)
    with (c/'traces'/f'{arm}.transitive.tsv').open() as stream:
        phases[arm]=read_phase(stream,'tactix' if arm=='candidate' else 'kissat',m['diagnostic_conflict'])
    print(arm,'full replay and census framing PASS',flush=True)
def first(a,b,fields):
    n=0
    while n<min(len(a),len(b)) and all(a[n][k]==b[n][k] for k in fields):n+=1
    return dict(matching_rows=n,complete_equal=n==len(a)==len(b),fields=fields,
        candidate=a[max(0,n-2):n+3],kissat=b[max(0,n-2):n+3])
sources={a:[s['begin'] for s in p['sources']] for a,p in phases.items()}
candidates={a:[r for s in p['sources'] for r in s['candidates']] for a,p in phases.items()}
reduced={a:reduced_pairs(p) for a,p in phases.items()}
assert sum(reduced['candidate'].values())==1 and sum(reduced['kissat'].values())==2,'Boundary/census reduction mismatch'
def multiset(counter):return [dict(pair=list(pair),copies=n) for pair,n in sorted(counter.items())]
result=dict(parent_replay=True,conflict=m['diagnostic_conflict'],
    counts={a:dict(all_trace_rows=p['total_rows'],all_sources=p['total_sources'],
        phase_sources=len(p['sources']),phase_candidates=len(candidates[a])) for a,p in phases.items()},
    reduced={a:multiset(v) for a,v in reduced.items()},
    candidate_only_reductions=multiset(reduced['candidate']-reduced['kissat']),
    kissat_only_reductions=multiset(reduced['kissat']-reduced['candidate']),
    first_source_difference=first(sources['candidate'],sources['kissat'],['source','candidate_count','out_degree']),
    first_candidate_difference=first(candidates['candidate'],candidates['kissat'],['source','destination','candidate_ordinal','redundant','failed']),
    hashes=hashes,
    limitation='Complete same-pin per-arm trace/proof replay, then framed ALL-source census. A differing candidate/order/outcome is localization, not a causal rule fix, full graph identity, final proof or timing acceptance.')
(c/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='hashes'},indent=2))
