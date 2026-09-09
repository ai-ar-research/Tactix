#!/usr/bin/env python3
"""Predeclare three matched full-runtime Rank50 waves after the causal witness."""
import csv, datetime, json, shutil, sys
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c=root/'benchmarks/runs/20260909-6592-rank50-material-three-wave-runtime'
assert not c.exists()
old=root/'benchmarks/runs/20260909-6584-material-schedule-BC-gate'
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items(): assert sha(old/rel)==digest,rel
gate=json.loads((old/'summary-6586.json').read_text()); assert gate['solver_records']==66
witness=root/'benchmarks/runs/20260909-6587-rank50-material-schedule/summary-6589.json'
w=json.loads(witness.read_text()); assert w['observer_neutral'] and w['decision_difference']['complete_equal']
full=root/'benchmarks/runs/20260909-6439-activation-ranks1-50'
oracle=full/'results/049.json'; r=json.loads(oracle.read_text())
assert r['rank']==50 and r['expected']=='SAT' and classify(r,True)['checked']
fm=json.loads((full/'manifest.json').read_text())
for name in ('pins','logs','results','provisional','claims'): (c/name).mkdir(parents=True)
for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):
    shutil.copy2(old/'pins'/name,c/'pins'/name)
arms={}; authorities={}
for arm,stem in (('candidate','6579-material-schedule'),('previous','6424-activation-stamp')):
    authority=root/f'benchmarks/runs/20260909-{stem}-build/manifest.json'
    b=json.loads(authority.read_text())
    for rel,digest in b['files'].items(): assert sha(root/b['source']/rel)==digest,rel
    binary=root/f'build/pin-{stem}-release/tactix'
    assert sha(binary)==b['binaries'][str(binary.relative_to(root))]
    shutil.copy2(binary,c/'pins'/arm)
    shutil.copy2(authority.parent/'cumulative.patch',c/'pins'/(arm+'.patch'))
    arms[arm]=dict(path='pins/'+arm,sha256=sha(binary),arguments=[],source_commit=b['source_commit'],
        source_build=str(binary.parent.relative_to(root)),source_patch='pins/'+arm+'.patch',source_patch_sha256=sha(c/'pins'/(arm+'.patch')))
    authorities[arm]=dict(path=str(authority.relative_to(root)),sha256=sha(authority))
rows=[]
for wave in range(3):
    for arm in (('previous','candidate') if wave==1 else ('candidate','previous')):
        rows.append(dict(task=len(rows),arm=arm,rank=50,tier='D',instance=r['instance'],expected='SAT'))
assert len(rows)==6
with (c/'matrix.tsv').open('x',newline='') as out:
    writer=csv.DictWriter(out,fieldnames=rows[0],delimiter='\t',lineterminator='\n'); writer.writeheader(); writer.writerows(rows)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('--array=0-65','--array=0-5').replace('6584-material-schedule-BC','6592-rank50-material-runtime'))
m.update(schema='tactix-material-three-wave-runtime-v1',arms=arms,authorities=authorities,
    input_sha256={r['instance']:fm['input_sha256'][r['instance']]},
    resource_contract=dict(m['resource_contract'],array='0-5'),
    previous_oracle=dict(path=str(oracle.relative_to(root)),sha256=sha(oracle)),
    dependencies={str(p.relative_to(root)):sha(p) for p in (old/'summary-6586.json',witness)},
    purpose='Price the successful general Rank50 material scheduling repair against its immutable6424 control.',
    hypothesis='Restoring the823562 material invitation reduces full Rank50 search work and runtime. No absolute gain assumed from prefix equality.',
    submission_contract='Three sequential independent full-width two-task waves0-1,2-3,4-5, middle order reversed; proceed after expected solves without waiting for checks.',
    acceptance='All6 expected checked/clean outcomes; previous arm exact to6439 and candidate later waves exact to its first wave. Report three paired ratios and each arm range/median; absolute timing claim requires both spreads<=2%. No subset promotion or fixed-reference refresh.',
    candidate_oracle='Task0 first wave is the new full-work/proof authority, frozen and bound before subsequent waves.',
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat())
m['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*sorted((c/'pins').iterdir()),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as out: json.dump(m,out,indent=2)
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
