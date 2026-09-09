#!/usr/bin/env python3
"""Fresh full50 reevaluation of accepted production and current pinned Kissat."""
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
selection=root/'benchmarks/baselines/rank1-50-selection-v1'
sm=json.loads((selection/'manifest.json').read_text())
assert sha(selection/'matrix.tsv')==sm['matrix_sha256']
with (selection/'matrix.tsv').open(newline='') as stream:
    inputs=list(csv.DictReader(stream,delimiter='\t'))
assert [int(r['rank']) for r in inputs]==list(range(1,51))
assert len({r['instance'] for r in inputs})==50
assert sum(r['expected']=='UNSAT' for r in inputs)==25
parent=root/'benchmarks/runs/20260908-6108-rank41-50-matched'
pm=json.loads((parent/'manifest.json').read_text())
accepted=root/'benchmarks/runs/20260908-6114-substitute-publication-BC-gate'
am=json.loads((accepted/'manifest.json').read_text())
ta=am['arms']['current_B']
assert ta['sha256']==am['arms']['current_C']['sha256']=='de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f'
assert ta['source_commit']=='c2a95a6c79c21a76516cf556b6538e76da369c34'
subprocess.run(['git','diff','--exit-code',ta['source_commit'],'--','src','include','CMakeLists.txt'],cwd=root,check=True)
subprocess.run(['git','diff','--exit-code','HEAD','--','src'],cwd=root/'kissat',check=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=root/'kissat',text=True).strip()==pm['arms']['kissat']['source_commit']
assert sha(root/'kissat/build/kissat')==pm['arms']['kissat']['sha256']
for campaign,m in ((parent,pm),(accepted,am)):
    for rel,digest in m['pin_sha256'].items():assert sha(campaign/rel)==digest,rel
for r in inputs:assert sha(root/'benchmarks/cache'/r['instance'])==sm['input_sha256'][r['instance']]
c=root/'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50'
assert not c.exists()
for name in ('pins','logs','results','provisional','claims'):(c/name).mkdir(parents=True)
for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py','kissat'):
    shutil.copy2(parent/'pins'/name,c/'pins'/name)
shutil.copy2(accepted/ta['path'],c/'pins/tactix')
rows=[dict(task=2*i+j,rank=r['rank'],arm=arm,tier='D',instance=r['instance'],expected=r['expected'])
      for i,r in enumerate(inputs) for j,arm in enumerate(('tactix','kissat'))]
assert [r['task'] for r in rows]==list(range(100))
for i in range(50):
    assert rows[2*i]['rank']==rows[2*i+1]['rank']==str(i+1)
with (c/'matrix.tsv').open('x',newline='') as stream:
    w=csv.DictWriter(stream,fieldnames=rows[0],delimiter='\t',lineterminator='\n')
    w.writeheader();w.writerows(rows)
batch=(parent/'array.sbatch').read_text().replace(str(parent),str(c))
assert batch.count('#SBATCH --array=0-19')==1
batch=batch.replace('#SBATCH --array=0-19','#SBATCH --array=0-99').replace('6108-ranks41-50','6121-fresh-accepted50')
(c/'array.sbatch').write_text(batch)
protected=dict(pm['protected_campaign_hashes'])
for old in (parent,accepted,root/'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50'):
    for name in ('manifest.json','array.sbatch'):
        p=old/name;protected[str(p.relative_to(root))]=sha(p)
for name in ('rank1-40-kissat-12cpu-v1','rank1-50-kissat-12cpu-v1'):
    p=root/'benchmarks/baselines'/name/'manifest.json';protected[str(p.relative_to(root))]=sha(p)
m=dict(schema='tactix-fresh-accepted-rank50-v1',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=dict(tactix=dict(ta,path='pins/tactix'),kissat=pm['arms']['kissat']),
    input_sha256=sm['input_sha256'],collect_runtime_context=True,
    resource_contract=dict(pm['resource_contract'],array='0-99'),
    selection=dict(path=str((selection/'manifest.json').relative_to(root)),sha256=sha(selection/'manifest.json')),
    accepted_authority=dict(path=str((accepted/'manifest.json').relative_to(root)),sha256=sha(accepted/'manifest.json'),arm='current_B'),
    authorization='User explicitly requested a completely fresh Rank1-50 Tactix-versus-current-Kissat reevaluation using accepted immutable binaries. Current repository production isc2a95a6;6107 remains an isolated investigation.',
    purpose='Fresh paired correctness/memory/runtime baseline;classify every invalidity and solve loss before scoring. Preserve fixed cross-window references and all established isolated repairs.',
    priority=['Tactix OOM/excess memory','Rank4 UNSAT','Rank32','portable convergence'],
    validation='All100 tasks must have independently checked original-formula models/proofs and zero residue before complete acceptance. Provisional results support ongoing investigation only. No exclusion silently removes a solve loss.',
    repetition='Full-width full-view array; targeted decision-grade performance repairs still require three paired waves. This run does not replace the fixed denominator or promote any source.',
    test_consideration='No solver,checker,runner or membership edit. Hash-identical checked runner retained;exact100-task paired mapping,50unique inputs,25SAT25UNSAT and immutable source/binary/protected-campaign assertions validate preparation.',
    protected_campaign_hashes=protected)
m['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode&~0o222)
for rel,digest in protected.items():assert sha(root/rel)==digest,rel
print(c)
