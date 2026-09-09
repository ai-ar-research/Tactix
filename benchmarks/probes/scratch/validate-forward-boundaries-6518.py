#!/usr/bin/env python3
"""Validate four neutral replays before comparing closure and forward-cleanup states."""
from collections import Counter
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from congruence_stages import read_formula
from binary_snapshot import read_snapshots,represented_values
campaigns={phase:root/f'benchmarks/runs/20260909-6516-forward-{phase}' for phase in ('before','after')}
target=campaigns['before']/'comparison-6518.json'; assert not target.exists()
if any(not (c/'results'/f'{arm}.json').exists() for c in campaigns.values() for arm in ('candidate','kissat')):
    print('Forward pair(s) pending; no output created'); sys.exit(0)
hashes={}; formulas={}; snapshots={}; value_maps={}
def bind(p):
    digest=sha(p); hashes[str(p.relative_to(root))]=digest; return digest
for phase,c in campaigns.items():
    m=json.loads((c/'manifest.json').read_text())
    assert m['phase']==phase and m['formula_ordinal']==(phase=='after')
    for rel,digest in m['hashes'].items(): assert bind(c/rel)==digest,rel
    for binding in [*m['authorities'].values(),m['parent_summary']]: assert bind(root/binding['path'])==binding['sha256']
    bind(c/'manifest.json'); bind(c/'launch.json')
    for arm,solver in (('candidate','tactix'),('kissat','kissat')):
        p=c/'results'/f'{arm}.json'; parent=root/m['parent_runs'][arm]; q=parent/'results'/p.name
        r=json.loads(p.read_text()); old=json.loads(q.read_text()); bind(p); bind(q)
        assert r['arm']==arm and r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
        assert r['binary_sha256']==old['binary_sha256']==sha(c/'pins'/arm)
        assert r['partial_certificate']==old['partial_certificate']
        assert set(r['trace_sha256'])-set(old['trace_sha256'])=={f'{arm}.forward-binaries.tsv'}
        for name,digest in old['trace_sha256'].items():
            if name==f'{arm}.formula.tsv': continue
            assert bind(parent/'traces'/name)==digest==r['trace_sha256'][name],name
        for name,digest in r['trace_sha256'].items(): assert bind(c/'traces'/name)==digest,name
        boundaries=(('slot_boundary','name=congruence-dense-resume|stage=second-entry'),
                    ('slot_boundary','name=congruence-dense-resume|stage=second')) if arm=='candidate' else (
                    ('congruence_dense_second_entry','-'),('congruence_dense_second','-'))
        with (c/'traces'/f'{arm}.forward-binaries.tsv').open() as f:
            snapshots[phase,arm]=read_snapshots(f,solver,227617,boundaries)
        assert len(snapshots[phase,arm])==4
        with (c/'traces'/f'{arm}.formula.tsv').open() as f:
            formulas[phase,arm]=list(read_formula(f,solver))
        assert formulas[phase,arm]
        value_maps[phase,arm]=represented_values(formulas[phase,arm])
        print(phase,arm,'full own1M trace/census/partial-proof replay PASS',flush=True)
for arm in ('candidate','kissat'):
    a,b=[c/'traces'/f'{arm}.forward-binaries.tsv' for c in campaigns.values()]
    assert sha(a)==sha(b), 'binary snapshots differ between neutral campaigns'
def diff(a,b):
    x,y=Counter(a),Counter(b); left,right=x-y,y-x
    return dict(equal_multiset=x==y,left_count=sum(x.values()),right_count=sum(y.values()),
        left_only_count=sum(left.values()),right_only_count=sum(right.values()),
        left_only_examples=list(sorted(left.items()))[:20],right_only_examples=list(sorted(right.items()))[:20])
def root_comparison(a,b):
    common=a.keys()&b.keys()
    changed=[(v,a[v],b[v]) for v in sorted(common) if a[v]!=b[v]]
    return dict(common_variables=len(common),left_only_variables=len(a.keys()-b.keys()),right_only_variables=len(b.keys()-a.keys()),
        unequal_common_values=len(changed),examples=changed[:40],
        limitation='Only variables present in large snapshots; not a complete root trail or unit-order comparison.')
def unique_disposition(a,b):
    other=Counter(r[0] for r in b); selected=[]
    for row in a:
        if other[row[0]]: other[row[0]]-=1
        else: selected.append(row)
    categories=Counter('root-satisfied' if 1 in row[2] else 'contains-false' if -1 in row[2] else 'all-unassigned' for row in selected)
    return dict(count=len(selected),categories=dict(categories),examples=selected[:20])
binary=[]
for i in range(4):
    binary.append(dict(call=i//2,stage='after' if i%2 else 'before',
        **diff(snapshots['before','candidate'][i]['rows'],snapshots['before','kissat'][i]['rows'])))
large={}
for phase in campaigns:
    a,b=formulas[phase,'candidate'],formulas[phase,'kissat']
    large[phase]=dict(identities=diff([r[0] for r in a],[r[0] for r in b]),
        identities_and_pose=diff([(r[0],r[1]) for r in a],[(r[0],r[1]) for r in b]),
        identities_and_values=diff([(r[0],r[2]) for r in a],[(r[0],r[2]) for r in b]),
        candidate_only=unique_disposition(a,b),kissat_only=unique_disposition(b,a),
        root_values=root_comparison(value_maps[phase,'candidate'],value_maps[phase,'kissat']),
        counts={arm:dict(Counter('red' if r[0][0] else 'irred' for r in formulas[phase,arm])) for arm in ('candidate','kissat')})
transitions={}
for arm in ('candidate','kissat'):
    a,b=formulas['before',arm],formulas['after',arm]
    transitions[arm]=dict(identities=diff([r[0] for r in a],[r[0] for r in b]),
        removed=unique_disposition(a,b),added=unique_disposition(b,a),
        root_values=root_comparison(value_maps['before',arm],value_maps['after',arm]))
out=dict(rank=33,conflict=227617,parent_replay=True,cross_campaign_binary_snapshots_exact=True,
    binary_comparisons=binary,first_call_large=large,forward_transitions=transitions,hashes=hashes,
    limitation='Aligned sparse boundaries before/after forward cleanup. Clause/pose/value multiset and represented-variable comparisons do not establish complete watch, representative or unit-order identity. Root-satisfied differences are not automatically trajectory-neutral. Bounded proof hash/cleanup is not final certificate acceptance or timing.')
target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(dict(binary=[{k:v for k,v in r.items() if not k.endswith('examples')} for r in binary],
    large={p:{k:v for k,v in r.items() if k in ('counts','root_values')} for p,r in large.items()},
    transitions={a:{k:v for k,v in r.items() if k=='root_values'} for a,r in transitions.items()}),indent=2))
