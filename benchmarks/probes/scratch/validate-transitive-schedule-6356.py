#!/usr/bin/env python3
"""Require ordinary/selected parent replay, then compare actual sort snapshots."""
from collections import Counter
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from transitive_schedule import FIELDS,read_schedule,sorted_under_recorded_keys
c=root/'benchmarks/runs/20260909-6352-rank32-transitive-schedule'
target=c/'summary.json';assert not target.exists()
subprocess.run(['python3','-m','unittest','discover','-s','tools/bench','-p','test_transitive_schedule.py'],cwd=root,check=True)
if not all((c/'results'/f'{a}.json').exists() for a in ('candidate','kissat')):
    print('Both completed records required; no partial interpretation.');sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(c/rel)==digest,rel
binding=m['parent_summary'];assert sha(root/binding['path'])==binding['sha256']
authority=m['candidate_authority'];assert sha(root/authority['path'])==authority['sha256']
build=json.loads((root/authority['path']).read_text())
for source,files in build['files'].items():
    for rel,digest in files.items():assert sha(root/source/rel)==digest
hashes={str((c/n).relative_to(root)):sha(c/n) for n in ('manifest.json','launch.json')}
observed={}
for arm in ('candidate','kissat'):
    parent=root/m['parent_runs'][arm]
    q=c/'results'/f'{arm}.json';r=json.loads(q.read_text())
    p=parent/'results'/q.name;before=json.loads(p.read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['arm']==arm and r['binary_sha256']==sha(c/'pins'/arm)
    assert r['partial_certificate']==before['partial_certificate']
    assert set(r['trace_sha256'])-set(before['trace_sha256'])=={arm+'.schedule.tsv'}
    assert set(before['trace_sha256'])<=set(r['trace_sha256'])
    for name,digest in before['trace_sha256'].items():assert sha(parent/'traces'/name)==digest==r['trace_sha256'][name],name
    for name,digest in r['trace_sha256'].items():
        path=c/'traces'/name;assert sha(path)==digest;hashes[str(path.relative_to(root))]=digest
    for path in (p,q):hashes[str(path.relative_to(root))]=sha(path)
    with (c/'traces'/f'{arm}.schedule.tsv').open() as stream:observed[arm]=read_schedule(stream)
    assert all(s['conflict']==m['diagnostic_conflict'] for s in observed[arm])
    print(arm,'ordinary+selected parent trace/proof replay and schedule framing PASS',flush=True)
comparisons=[]
for call in range(min(len(v) for v in observed.values())//2):
    before={a:v[2*call] for a,v in observed.items()}
    after={a:v[2*call+1] for a,v in observed.items()}
    inputs={a:{r['compact']:r for r in s['rows']} for a,s in before.items()}
    common=set(inputs['candidate'])&set(inputs['kissat'])
    differences=[];fields=Counter()
    for compact in sorted(common):
        a,b=inputs['candidate'][compact],inputs['kissat'][compact]
        keys=[k for k in FIELDS[1:] if a[k]!=b[k]]
        fields.update(keys)
        if keys and len(differences)<20:differences.append(dict(compact=compact,fields=keys,candidate=a,kissat=b))
    a,b=after['candidate']['rows'],after['kissat']['rows']
    n=0
    while n<min(len(a),len(b)) and (a[n]['compact'],a[n]['external'])==(b[n]['compact'],b[n]['external']):n+=1
    subject={}
    for arm,rows in ((arm,s['rows']) for arm,s in after.items()):
        found=[(pos,r) for pos,r in enumerate(rows) if abs(r['external'])==936]
        subject[arm]=[dict(row=r,ascending_position=pos,reverse_variable_position=len(rows)-1-pos) for pos,r in found]
    comparisons.append(dict(call=call,entry={a:{k:s[k] for k in ('stable','decisions','ticks','size')} for a,s in before.items()},
        same_compact_membership=set(inputs['candidate'])==set(inputs['kissat']),
        candidate_only_compacts=sorted(set(inputs['candidate'])-set(inputs['kissat'])),
        kissat_only_compacts=sorted(set(inputs['kissat'])-set(inputs['candidate'])),
        differing_input_fields=dict(fields),first_input_differences=differences,
        sorted_under_recorded_keys={a:sorted_under_recorded_keys(s) for a,s in after.items()},
        nonzero_priorities={a:sum(r['priority']!=0 for r in s['rows']) for a,s in before.items()},
        matching_ascending_sources=n,complete_source_order_equal=n==len(a)==len(b),
        first_ascending_difference=dict(candidate=a[max(0,n-2):n+3],kissat=b[max(0,n-2):n+3]),subject936=subject))
result=dict(parent_replay=True,conflict=m['diagnostic_conflict'],snapshots={a:len(v) for a,v in observed.items()},
    comparisons=comparisons,hashes=hashes,
    limitation='Read-only actual before/after inputs preserved per arm; ascending sort is consumed in reverse with both polarities. Differences in membership or inactive key state require producer localization before a fix. Source936 positions are not proof of actual budget reach or later search causality. No timing or final-proof acceptance.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='hashes'},indent=2))
