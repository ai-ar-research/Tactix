#!/usr/bin/env python3
"""Complete current/previous prefix comparison with independent old-closure check."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, first_stream_difference
c = root/'benchmarks/runs/20260909-6376-rank35-stamp-prefix'
target = c/'summary.json'
assert not target.exists()
if any(not (c/'results'/f'{arm}.json').exists() for arm in ('candidate','previous')):
    print('Rank35 prefix still pending; no summary created')
    raise SystemExit(0)
m = json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items():
    assert sha(c/rel)==digest
hashes = {str((c/name).relative_to(root)):sha(c/name) for name in ('manifest.json','launch.json')}
prefixes = {}; closures = {}
for arm in ('candidate','previous'):
    p = c/'results'/f'{arm}.json'
    r = json.loads(p.read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==sha(c/'pins'/arm) and r['partial_certificate']['bytes']>0
    hashes[str(p.relative_to(root))]=sha(p)
    for rel,digest in r['trace_sha256'].items():
        p=c/'traces'/rel; assert sha(p)==digest
        hashes[str(p.relative_to(root))]=digest
    prefix=read_prefix(c/'traces'/f'{arm}.prefix.tsv')
    assert prefix['conflict']==m['stop_conflict'] and prefix['common']==m['common_decisions']
    stream=c/'traces'/f'{arm}.decisions.tsv'
    count=int(subprocess.check_output(['wc','-l',str(stream)],text=True).split()[0])
    assert count==prefix['count']
    last=list(map(int,subprocess.check_output(['tail','-n','1',str(stream)],text=True).split()))
    first=list(map(int,subprocess.check_output(['head','-n','1',str(stream)],text=True).split()))
    assert len(first)==len(last)==5 and first[3]==prefix['first'] and last[3]==prefix['last']
    assert last[1]==prefix['end_decisions'] and last[0]<=m['stop_conflict']
    assert prefix['start_decisions']==(prefix['end_decisions']-prefix['count'])%(1<<64)
    prefixes[arm]=prefix
    closures[arm]=prefix['common_reached']==1 and prefix['common_hash']==m['prior_common_hash']
difference=first_stream_difference(c/'traces/candidate.decisions.tsv',c/'traces/previous.decisions.tsv')
same_hash=all(prefixes['candidate'][k]==prefixes['previous'][k] for k in ('count','hash','end_decisions','first','last'))
assert difference['complete_equal']==same_hash
result=dict(rank=35,stop=m['stop_conflict'],prefixes=prefixes,prior_rephase_closure=closures,
    current_previous_decisions=difference,hashes=hashes,
    limitation='Full selected-decision streams compared through100K; unchanged or changed prior closure is reported rather than assumed. No whole-formula identity, final proof acceptance or timing claim.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ('prior_rephase_closure','current_previous_decisions')},indent=2))
