#!/usr/bin/env python3
"""Require exact parent replay before interpreting the selected Vivify phase."""
import itertools
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from vivify_trace import read_rounds, read_candidates
c=root/'benchmarks/runs/20260909-6483-rank35-vivify-replay'
target=c/'summary.json'; assert not target.exists()
arms=('candidate','previous')
if any(not (c/'results'/f'{a}.json').exists() for a in arms):
    print('Rank35 Vivify replay pending; no output created'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest
binding=m['parent_summary']; assert sha(root/binding['path'])==binding['sha256']
hashes={str((c/n).relative_to(root)):sha(c/n) for n in ('manifest.json','launch.json')}
rounds={}
for arm in arms:
    parent=root/m['parent_runs'][arm]
    p=c/'results'/f'{arm}.json'; q=parent/'results'/p.name
    r=json.loads(p.read_text()); old=json.loads(q.read_text())
    assert r['arm']==arm and r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==old['binary_sha256']==sha(c/'pins'/arm)
    assert r['partial_certificate']==old['partial_certificate']
    expected={arm+'.vivify-candidates.tsv',arm+'.vivify-rounds.tsv'}
    assert set(r['trace_sha256'])-set(old['trace_sha256'])==expected
    for name,digest in old['trace_sha256'].items(): assert sha(parent/'traces'/name)==digest==r['trace_sha256'][name]
    for name,digest in r['trace_sha256'].items():
        t=c/'traces'/name; assert sha(t)==digest; hashes[str(t.relative_to(root))]=digest
    for t in (p,q): hashes[str(t.relative_to(root))]=sha(t)
    with (c/'traces'/f'{arm}.vivify-rounds.tsv').open() as stream:
        rounds[arm]=read_rounds(stream,m['diagnostic_conflict'])
    print(arm,'exact own trace/partial-proof replay PASS',flush=True)
fields={
 'semantic':['round','event','conflicts','scope','ordinal','size','redundant','glue','priority','garbage','clause','pose'],
 'effort':['round','event','scope','ordinal','ticks','limit'],
 'physical_metadata':['round','event','scope','ordinal','ref','position'],
}
first={key:None for key in fields}; counts=dict.fromkeys(arms,0)
with (c/'traces/candidate.vivify-candidates.tsv').open() as a, (c/'traces/previous.vivify-candidates.tsv').open() as b:
    for index,(left,right) in enumerate(itertools.zip_longest(read_candidates(a,rounds['candidate']),read_candidates(b,rounds['previous']))):
        counts['candidate']+=left is not None; counts['previous']+=right is not None
        for name,columns in fields.items():
            projection=lambda r: None if r is None else {k:r[k] for k in columns}
            if first[name] is None and projection(left)!=projection(right):
                first[name]=dict(index=index,fields=columns,candidate=left,previous=right)
result=dict(rank=35,conflict=m['diagnostic_conflict'],parent_replay=True,rounds=rounds,
    candidate_rows=counts,first_differences=first,hashes=hashes,
    limitation='Whole selected logs and own stopped trace/proof replay validated. First candidate/effort differences are localization, not formula equivalence, causal repair, final proof acceptance or runtime evidence.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(candidate_rows=counts,first_differences=first),indent=2))
