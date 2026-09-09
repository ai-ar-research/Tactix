#!/usr/bin/env python3
"""Predeclare three matched changed-work waves on productive and adverse cases."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c=root/'benchmarks/runs/20260909-6332-collapse-three-wave-runtime'
assert not c.exists()
old=root/'benchmarks/runs/20260909-6176-learned-sort-rank4-runtime'
om=json.loads((old/'manifest.json').read_text())
for rel,digest in om['pin_sha256'].items():assert sha(old/rel)==digest
gate=root/'benchmarks/runs/20260909-6311-substitute-collapse-BC-gate'
records={}
for folder in ('provisional','results'):
    for p in (gate/folder).glob('*.json'):
        r=json.loads(p.read_text());assert classify(r,folder=='results')['status'] in ('pending_validation','checked_SAT','checked_UNSAT')
        records[r['task']]=p
assert set(records)==set(range(66))
arms={};authorities={};oracles={};inputs={};seed={}
for arm,number,stem,run in (
    ('candidate',6305,'substitute-collapse','20260909-6320-substitute-collapse-ranks1-50'),
    ('previous',6269,'vivify-binary-retry','20260909-6278-vivify-binary-ranks1-50')):
    a=root/f'benchmarks/runs/20260909-{number}-{stem}-build/manifest.json'
    b=json.loads(a.read_text())
    for rel,digest in b['files'].items():assert sha(root/b['source']/rel)==digest
    pin=root/f'build/pin-{number}-{stem}-release/tactix'
    assert sha(pin)==b['binaries'][str(pin.relative_to(root))]
    arms[arm]=dict(path='pins/'+arm,sha256=sha(pin),arguments=[],source_commit=b['source_commit'],source_build=str(pin.parent.relative_to(root)))
    authorities[arm]={'path':str(a.relative_to(root)),'sha256':sha(a)}
    oracles[arm]={}
    rm=json.loads((root/'benchmarks/runs'/run/'manifest.json').read_text())
    for rank in (32,33,40):
        p=root/'benchmarks/runs'/run/'results'/f'{rank-1:03}.json'
        if not p.exists():p=p.parent.parent/'provisional'/p.name
        r=json.loads(p.read_text());assert classify(r,p.parent.name=='results')['status'] in ('pending_validation','checked_UNSAT')
        assert r['rank']==rank and r['expected']=='UNSAT'
        oracles[arm][str(rank)]={'path':str(p.relative_to(root)),'sha256':sha(p)}
        inputs[r['instance']]=rm['input_sha256'][r['instance']]
        seed[rank]=dict(rank=rank,tier='D',instance=r['instance'],expected=r['expected'])
for name in ('pins','logs','results','provisional','claims'):(c/name).mkdir(parents=True)
for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):shutil.copy2(old/'pins'/name,c/'pins'/name)
for arm,a in arms.items():shutil.copy2(root/a['source_build']/'tactix',c/a['path'])
rows=[]
for wave in range(3):
    for rank in (32,33,40):
        for arm in (('previous','candidate') if wave==1 else ('candidate','previous')):
            rows.append(dict(task=len(rows),arm=arm,**seed[rank]))
assert len(rows)==18 and all({r['arm'] for r in rows[i:i+2]}=={'candidate','previous'} for i in range(0,18,2))
with (c/'matrix.tsv').open('x',newline='') as out:
    w=csv.DictWriter(out,fieldnames=rows[0],delimiter='\t',lineterminator='\n');w.writeheader();w.writerows(rows)
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('--array=0-5','--array=0-17').replace('6176-sort-rank4','6332-collapse-timing')
(c/'array.sbatch').write_text(batch)
m=dict(schema='tactix-collapse-changed-work-three-waves-v1',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms,authorities=authorities,input_sha256=inputs,collect_runtime_context=True,
    resource_contract=dict(om['resource_contract'],array='0-17'),protected_campaign_hashes=om['protected_campaign_hashes'],
    work_oracles=oracles,bc_solver_records={str(p.relative_to(root)):sha(p) for p in records.values()},
    witnesses={'32':'primary Rank32 search reduction','33':'second productive changed-work family','40':'observed adverse search-work control'},
    submission_contract='Three sequential full-width six-task waves0-5,6-11,12-17; expected solver outcomes permit next wave without waiting proofs. Middle wave reverses arm order. No node constraints or throttles.',
    hypothesis='6305 retained-endpoint retry causes repeatable changed-work runtime effects versus6269, including the adverseRank40 case.',
    acceptance='All18 expected independently checked clean outcomes; every arm exact to its own work/proof oracle. Report per-rank median/range of three paired ratios and each arm spread; absolute claim requires <=2% spread. No fixed50 competitiveness or promotion from subset.',
    test_consideration='Unchanged pinned runner/oracles; existing39 benchmark13watcher tests. Matrix validates18tasks,pairedarmcoverage; source gates105/104,72fixtures,8proofs alreadypassed. New timing compares different search work,not isolated instruction cost.',
    limitation='Source fuzz,memory and full50 gates remain separate. No new Kissat run or fixed-reference change.')
m['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
