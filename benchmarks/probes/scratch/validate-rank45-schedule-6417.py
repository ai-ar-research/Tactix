#!/usr/bin/env python3
"""Validate exact three-arm replay before comparing every recorded sort key."""
from collections import Counter
import itertools
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from transitive_schedule import FIELDS,read_schedule,sorted_under_recorded_keys
c=root/'benchmarks/runs/20260909-6415-rank45-transitive-schedule'
target=c/'summary.json'; assert not target.exists()
arms=('candidate','previous','kissat')
if any(not (c/'results'/f'{a}.json').exists() for a in arms):
    print('Three completed records required; no output created'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest
for b in [m['parent_summary'],*m['authorities'].values()]: assert sha(root/b['path'])==b['sha256']
hashes={str((c/n).relative_to(root)):sha(c/n) for n in ('manifest.json','launch.json')}
observed={}
for arm in arms:
    parent=root/m['parent_runs'][arm]
    p=c/'results'/f'{arm}.json'; q=parent/'results'/p.name
    r=json.loads(p.read_text()); old=json.loads(q.read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['arm']==arm and r['binary_sha256']==sha(c/'pins'/arm)
    assert r['partial_certificate']==old['partial_certificate']
    assert set(r['trace_sha256'])-set(old['trace_sha256'])=={arm+'.schedule.tsv'}
    assert set(old['trace_sha256'])<=set(r['trace_sha256'])
    for name,digest in old['trace_sha256'].items(): assert sha(parent/'traces'/name)==digest==r['trace_sha256'][name],name
    for name,digest in r['trace_sha256'].items():
        t=c/'traces'/name; assert sha(t)==digest; hashes[str(t.relative_to(root))]=digest
    for t in (p,q): hashes[str(t.relative_to(root))]=sha(t)
    with (c/'traces'/f'{arm}.schedule.tsv').open() as stream: observed[arm]=read_schedule(stream)
    assert len(observed[arm])==4
    assert all(s['conflict']==76464 for s in observed[arm])
    print(arm,'full trace/proof replay and four snapshot frames PASS',flush=True)
comparisons=[]
for call in range(2):
    before={a:observed[a][2*call] for a in arms}; after={a:observed[a][2*call+1] for a in arms}
    inputs={a:{r['compact']:r for r in s['rows']} for a,s in before.items()}
    pairs={}
    for a,b in itertools.combinations(arms,2):
        common=set(inputs[a])&set(inputs[b]); diffs=[]; fields=Counter()
        for compact in sorted(common):
            x,y=inputs[a][compact],inputs[b][compact]
            keys=[k for k in FIELDS[1:] if x[k]!=y[k]]; fields.update(keys)
            if keys: diffs.append(dict(compact=compact,fields=keys,left=x,right=y))
        x,y=after[a]['rows'][::-1],after[b]['rows'][::-1]; n=0
        while n<min(len(x),len(y)) and (x[n]['compact'],x[n]['external'])==(y[n]['compact'],y[n]['external']): n+=1
        pairs[a+'_'+b]=dict(left_only_compacts=sorted(set(inputs[a])-set(inputs[b])),
            right_only_compacts=sorted(set(inputs[b])-set(inputs[a])),differing_input_fields=dict(fields),
            input_differences=diffs,matching_reverse_sources=n,complete_source_order_equal=n==len(x)==len(y),
            first_reverse_difference=dict(left=x[max(0,n-2):n+3],right=y[max(0,n-2):n+3]))
    comparisons.append(dict(call=call,entry={a:{k:s[k] for k in ('stable','decisions','ticks','size')} for a,s in before.items()},
        pairs=pairs,sorted_under_recorded_keys={a:sorted_under_recorded_keys(s) for a,s in after.items()},
        nonzero_priorities={a:sum(r['priority']!=0 for r in s['rows']) for a,s in before.items()}))
result=dict(parent_replay=True,rank=45,conflict=76464,comparisons=comparisons,hashes=hashes,
    limitation='Actual read-only keys and full observer-neutral replay. Differences need producer localization; no formula identity, final certificate, timing or causal repair claim.')
target.write_text(json.dumps(result,indent=2)+'\n')
for row in comparisons:
    print('call',row['call'],'entry',row['entry'],'sorted',row['sorted_under_recorded_keys'])
    for pair,p in row['pairs'].items(): print(pair,'fields',p['differing_input_fields'],'equal reverse rows',p['matching_reverse_sources'],'complete',p['complete_source_order_equal'])
