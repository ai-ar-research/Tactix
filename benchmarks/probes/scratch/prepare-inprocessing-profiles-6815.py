#!/usr/bin/env python3
"""Matched current Release profiles for broad and Sweep-heavy UNSAT cost."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),
    '--deadline','2026-09-10T06:00:00-04:00'],check=True)
current=root/'benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50'
fresh=root/'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50'
protocol=root/'benchmarks/runs/20260908-5544-small-rank39-profile'
c=root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles'
assert not c.exists()
cm=json.loads((current/'manifest.json').read_text());fm=json.loads((fresh/'manifest.json').read_text())
pm=json.loads((protocol/'manifest.json').read_text())
for campaign,manifest in ((current,cm),(fresh,fm),(protocol,pm)):
    for rel,digest in manifest['pin_sha256'].items():assert sha(campaign/rel)==digest,rel
census=root/'benchmarks/runs/20260910-6814-inprocessing-cost-census/summary.json'
s=json.loads(census.read_text());assert s['selected_ranks']==[32,39]
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
classification=root/'benchmarks/runs/20260910-6804-rank21-analysis-admission/terminal-6813.json'
assert classification.exists()
deps=dict(cm.get('dependencies',{}))
for p in (census,classification,Path(__file__),current/'manifest.json',current/'launch.json',fresh/'manifest.json',protocol/'manifest.json'):
    deps[str(p.relative_to(root))]=sha(p)
records={}
for rank in (32,39):
    for arm,path in (('candidate',current/f'results/{rank-1:03d}.json'),('kissat',fresh/f'results/{2*rank-1:03d}.json')):
        record=json.loads(path.read_text());state=classify(record,True)
        assert state['checked'] and state['clean'] and record['rank']==rank and record['expected']=='UNSAT'
        expected=cm['arms']['tactix']['sha256'] if arm=='candidate' else fm['arms']['kissat']['sha256']
        assert record['binary_sha256']==expected
        records[f'{rank}/{arm}']=dict(path=str(path.relative_to(root)),sha256=sha(path))
for name in ('pins','logs','results','provisional','claims','profiles'):(c/name).mkdir(parents=True)
for name in ('runtime_task.py','checkmodel','modelcheck.py','drat-trim'):shutil.copy2(fresh/'pins'/name,c/'pins'/name)
shutil.copy2(current/'pins/tactix',c/'pins/candidate')
shutil.copy2(fresh/'pins/kissat',c/'pins/kissat')
shutil.copy2(protocol/'pins/run.py',c/'pins/run.py')
with (fresh/'matrix.tsv').open(newline='') as stream:
    reader=csv.DictReader(stream,delimiter='\t');fields=reader.fieldnames
    rows=[r for r in reader if int(r['rank']) in (32,39)]
assert [(r['rank'],r['arm']) for r in rows]==[('32','tactix'),('32','kissat'),('39','tactix'),('39','kissat')]
for task,row in enumerate(rows):
    row['task']=str(task)
    if row['arm']=='tactix':row['arm']='candidate'
with (c/'matrix.tsv').open('x',newline='') as stream:
    writer=csv.DictWriter(stream,fields,delimiter='\t',lineterminator='\n');writer.writeheader();writer.writerows(rows)
batch=(fresh/'array.sbatch').read_text().replace(str(fresh),str(c))
assert batch.count('--array=0-99')==1
batch=batch.replace('--array=0-99','--array=0-3').replace('/pins/runtime_task.py','/pins/run.py').replace('6121-fresh-accepted50','6815-inprocess-profiles')
(c/'array.sbatch').write_text(batch)
candidate=dict(cm['arms']['tactix'],path='pins/candidate')
m=dict(fm,schema='tactix-inprocessing-unit-cost-profiles-v1',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=dict(candidate=candidate,kissat=fm['arms']['kissat']),
    input_sha256={row['instance']:fm['input_sha256'][row['instance']] for row in rows},
    dependencies=deps,unprofiled_records=records,
    resource_contract=dict(fm['resource_contract'],array='0-3'),
    priority=['Inprocessing unit cost','Inprocessing scheduling and CDCL parity safeguards'],
    profile=dict(event='cycles:u',frequency=199,call_graph='dwarf,8192'),
    purpose='User-directed inprocessing unit-cost focus on current6730: Rank32 broad exact-full-conflict/decision control and Rank39 Sweep-heavy UNSAT control.',
    hypothesis='Inprocessing callpaths and self samples identify dominant repeated scanning, propagation, allocation/copying or scheduling-construction cost worth a general same-work optimization. Existing count-cache neutral result and unsupported Factor rewrite remain rejected as speedup explanations.',
    validation='All four expected UNSAT, exact own unprofiled certificate bytes/hash and work counters; candidate every non-timing statistic exact. Valid self/callpath reports, zero lost samples and hash-bound compressed data. Independent proofs and zero residue; pending checks permit development only.',
    parity='Cost changes must preserve current complete work/proofs and scheduling admission, effort/limits and CDCL mechanisms. Rank32 counters match native but do not prove whole-formula/propagation identity. Rank39 differs in work; percentages alone are not unit prices. Known31/35/21 search gaps remain open, stronger33 retained.',
    authorization='Bounded native diagnostic permitted by fixed50 policy; fixed7239.010s denominator untouched. No further Rank21 search probes per user reprioritization. No source promotion, FlatLists redesign, fuzz access or denied push/transfer retry.',
    test_consideration='Five new timer tests and seven source-key-preflight tests; maintained benchmark suite244 and watcher19. Sampling wrapper copied byte-for-byte from validated5544. Exact four-task matrix and within-arm replay protect unchanged protocol; no global event trace or stop setting is enabled.',
    repetition='One diagnostic four-task array, not timing acceptance. Any optimization needs three independent unprofiled paired waves and correctness/B28+C5/full50 gates appropriate to risk.')
m['protected_campaign_hashes']=dict(json.loads((current/'launch.json').read_text())['protected_campaign_hashes'])
m['current_authority']=dict(path=str((current/'manifest.json').relative_to(root)),sha256=sha(current/'manifest.json'))
m['prepare_failure']='First unsubmitted attempt failed before manifest publication because current6743 owns protected hashes in launch.json, not manifest.json. Partial files preserved in sibling prepare-attempt1; no solver ran and no accepted artifact was changed.'
for rel,digest in m['protected_campaign_hashes'].items():assert sha(root/rel)==digest,rel
m['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as stream:json.dump(m,stream,indent=2);stream.write('\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
