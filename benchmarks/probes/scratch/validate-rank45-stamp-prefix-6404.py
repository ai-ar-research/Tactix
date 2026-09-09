#!/usr/bin/env python3
"""Complete three-arm prefix comparison; no assumed parent/native identity."""
import itertools
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix,first_stream_difference
c=root/'benchmarks/runs/20260909-6402-rank45-stamp-prefix'
target=c/'summary.json'; assert not target.exists()
arms=('candidate','previous','kissat')
if any(not (c/'results'/f'{a}.json').exists() for a in arms):
    print('Rank45 prefix pending; no summary created'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest
hashes={str((c/n).relative_to(root)):sha(c/n) for n in ('manifest.json','launch.json')}
prefixes={}
for arm in arms:
    p=c/'results'/f'{arm}.json'; r=json.loads(p.read_text())
    assert r['arm']==arm and r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==sha(c/'pins'/arm) and r['partial_certificate']['bytes']>0
    hashes[str(p.relative_to(root))]=sha(p)
    for name,digest in r['trace_sha256'].items():
        p=c/'traces'/name; assert sha(p)==digest; hashes[str(p.relative_to(root))]=digest
    prefix=read_prefix(c/'traces'/f'{arm}.prefix.tsv')
    assert prefix['conflict']==100000 and prefix['common']==1 and prefix['common_reached']==1
    stream=c/'traces'/f'{arm}.decisions.tsv'
    count=int(subprocess.check_output(['wc','-l',str(stream)],text=True).split()[0])
    assert count==prefix['count'] and count>0
    first=list(map(int,subprocess.check_output(['head','-n','1',str(stream)],text=True).split()))
    last=list(map(int,subprocess.check_output(['tail','-n','1',str(stream)],text=True).split()))
    assert len(first)==len(last)==5 and first[3]==prefix['first'] and last[3]==prefix['last']
    assert last[1]==prefix['end_decisions'] and last[0]<=100000
    assert first[1]==(prefix['start_decisions']+1)%(1<<64)
    assert prefix['start_decisions']==(prefix['end_decisions']-count)%(1<<64)
    prefixes[arm]=prefix
pairs={}
for a,b in itertools.combinations(arms,2):
    difference=first_stream_difference(c/'traces'/f'{a}.decisions.tsv',c/'traces'/f'{b}.decisions.tsv')
    same=all(prefixes[a][k]==prefixes[b][k] for k in ('count','hash','end_decisions','first','last'))
    assert same==difference['complete_equal']
    pairs[a+'_'+b]=difference
result=dict(rank=45,stop=100000,prefixes=prefixes,pairs=pairs,hashes=hashes,
    limitation='First selected split or bounded equality only; not full-state/proof/cost identity, causal attribution or fixed-reference refresh. Partial proofs were hashed/deleted, not accepted final certificates.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(pairs,indent=2))
