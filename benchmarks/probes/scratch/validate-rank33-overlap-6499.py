#!/usr/bin/env python3
"""Correct6488 zero-overlap assumption without weakening common-prefix checks."""
import itertools
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from decision_overlap import threshold_overlap
from decision_prefix import read_prefix, validate_common, first_stream_difference
c=root/'benchmarks/runs/20260909-6486-rank33-later-prefix'
target=c/'summary.json'; assert not target.exists()
arms=('candidate','previous','kissat')
if any(not (c/'results'/f'{a}.json').exists() for a in arms):
    print('Rank33 later prefix pending; no output created'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest
binding=m['prior_closure']; assert sha(root/binding['path'])==binding['sha256']
old=json.loads((root/binding['path']).read_text())
assert all(p['complete_equal'] and p['matching_rows']==31494584 for p in old['pairs'].values())
hashes={str((c/n).relative_to(root)):sha(c/n) for n in ('manifest.json','launch.json')}
hashes[binding['path']]=binding['sha256']
prefixes={}; overlap_rows={}; suffix_rows={}; overlaps={}
for arm in arms:
    parent=root/m['parent_runs'][arm]
    stream=parent/'traces'/f'{arm}.decisions.tsv'
    assert sha(stream)==old['hashes'][str(stream.relative_to(root))]
    overlap=threshold_overlap(stream,100000)
    overlap_count=overlap.count(b'\n')
    assert all(row.startswith(b'100000\t') for row in overlap.splitlines())
    prior_last=list(map(int,subprocess.check_output(['tail','-n','1',str(stream)],text=True).split()))
    assert len(prior_last)==5 and prior_last[3]==old['prefixes'][arm]['last']
    assert prior_last[0]<=100000 and bool(overlap_count)==(prior_last[0]==100000)
    p=c/'results'/f'{arm}.json'; r=json.loads(p.read_text())
    assert r['arm']==arm and r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==sha(c/'pins'/arm) and r['partial_certificate']['bytes']>0
    hashes[str(p.relative_to(root))]=sha(p)
    for name,digest in r['trace_sha256'].items():
        p=c/'traces'/name; assert sha(p)==digest; hashes[str(p.relative_to(root))]=digest
    prefix=read_prefix(c/'traces'/f'{arm}.prefix.tsv')
    validate_common(prefix,stop=1000000,common=31494584,digest='5c263e00aa882466')
    assert prefix['common_last']==old['prefixes'][arm]['last']
    stream=c/'traces'/f'{arm}.decisions.tsv'
    with stream.open('rb') as inp: assert inp.read(len(overlap))==overlap,'complete overlap changed'
    first=list(map(int,subprocess.check_output(['head','-n','1',str(stream)],text=True).split()))
    assert len(first)==5 and first[0]>=100000
    if not overlap_count: assert first[0]>prior_last[0], 'continuation duplicates or precedes prior tail'
    count=int(subprocess.check_output(['wc','-l',str(stream)],text=True).split()[0])
    assert count==prefix['count']-31494584+overlap_count
    last=list(map(int,subprocess.check_output(['tail','-n','1',str(stream)],text=True).split()))
    assert len(last)==5 and last[0]<=1000000 and last[3]==prefix['last'] and last[1]==prefix['end_decisions']
    assert prefix['start_decisions']==(prefix['end_decisions']-prefix['count'])%(1<<64)
    prefixes[arm]=prefix; overlap_rows[arm]=overlap_count; suffix_rows[arm]=count; overlaps[arm]=overlap
assert len(set(overlaps.values()))==1,'cached common overlap differs across arms'
pairs={}
for a,b in itertools.combinations(arms,2):
    comparison=first_stream_difference(c/'traces'/f'{a}.decisions.tsv',c/'traces'/f'{b}.decisions.tsv')
    comparison['matching_decisions_from_start']=31494584+comparison['matching_rows']-overlap_rows[a]
    pairs[a+'_'+b]=comparison
result=dict(rank=33,stop=1000000,prefixes=prefixes,overlap_rows=overlap_rows,suffix_rows=suffix_rows,
    pairs=pairs,hashes=hashes,
    limitation='Bounded first decision split or equality after complete cached overlap validation. Not causal, formula/heap identity, fullsolve proof or timing acceptance. Partial proofs are hashed/deleted and fixed native timings unchanged.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(pairs,indent=2))
