#!/usr/bin/env python3
"""Validate no-overlap continuation before localizing an adverse decision split."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix,validate_common,first_stream_difference
c=root/'benchmarks/runs/20260909-6389-rank35-stamp-extension'
target=c/'summary.json'; assert not target.exists()
if any(not (c/'results'/f'{a}.json').exists() for a in ('candidate','previous')):
    print('Rank35 extension pending; no summary created'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest
binding=m['prior_closure']; prior=root/binding['path']; assert sha(prior)==binding['sha256']
old=json.loads(prior.read_text())
hashes={str((c/n).relative_to(root)):sha(c/n) for n in ('manifest.json','launch.json')}
hashes[str(prior.relative_to(root))]=sha(prior)
prefixes={}; counts={}
for arm in ('candidate','previous'):
    p=c/'results'/f'{arm}.json'; r=json.loads(p.read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==sha(c/'pins'/arm) and r['partial_certificate']['bytes']>0
    hashes[str(p.relative_to(root))]=sha(p)
    for name,digest in r['trace_sha256'].items():
        p=c/'traces'/name; assert sha(p)==digest; hashes[str(p.relative_to(root))]=digest
    prefix=read_prefix(c/'traces'/f'{arm}.prefix.tsv')
    validate_common(prefix,stop=1000000,common=40636523,digest='6a3ebeb3a46d7ad5')
    assert prefix['common_last']==old['prefixes'][arm]['last']
    stream=c/'traces'/f'{arm}.decisions.tsv'
    count=int(subprocess.check_output(['wc','-l',str(stream)],text=True).split()[0])
    assert count==prefix['count']-40636523 and count>0
    first=list(map(int,subprocess.check_output(['head','-n','1',str(stream)],text=True).split()))
    last=list(map(int,subprocess.check_output(['tail','-n','1',str(stream)],text=True).split()))
    assert len(first)==len(last)==5 and first[0]>=100000 and last[0]<=1000000
    assert first[1]==(old['prefixes'][arm]['end_decisions']+1)%(1<<64)
    assert last[1]==prefix['end_decisions'] and last[3]==prefix['last']
    assert prefix['start_decisions']==(prefix['end_decisions']-prefix['count'])%(1<<64)
    prefixes[arm]=prefix; counts[arm]=count
difference=first_stream_difference(c/'traces/candidate.decisions.tsv',c/'traces/previous.decisions.tsv')
same=all(prefixes['candidate'][k]==prefixes['previous'][k] for k in ('count','hash','end_decisions','first','last'))
assert difference['complete_equal']==same
result=dict(rank=35,stop=1000000,prefixes=prefixes,suffix_rows=counts,validated_overlap_rows=0,
    decisions=difference,matching_decisions_from_start=40636523+difference['matching_rows'],hashes=hashes,
    limitation='First selected split only, not causal explanation or full-state identity; unchanged100K prefix verified. Partial proofs hashed/deleted, not accepted final certificates.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ('decisions','matching_decisions_from_start')},indent=2))
