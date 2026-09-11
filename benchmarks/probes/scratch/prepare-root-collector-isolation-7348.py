#!/usr/bin/env python3
"""Three existing Release pins isolate the new B14 trajectory discrepancy."""
import csv
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c=root/'benchmarks/runs/20260911-7348-root-collector-isolation'
gate=root/'benchmarks/runs/20260911-7347-root-storage-BC'
assert not c.exists()
m=json.loads((gate/'manifest.json').read_text())
dependencies=dict(m['dependencies'])
def bind(path,expected=None):
    digest=sha(path); assert expected is None or digest==expected,path
    dependencies[str(path.relative_to(root))]=digest
for rel,digest in list(dependencies.items()): bind(root/rel,digest)
for rel,digest in m['pin_sha256'].items(): bind(gate/rel,digest)
bind(gate/'manifest.json'); bind(gate/'launch.json')
oracles={}
for task in (26,27):
    path=gate/f'results/{task:03d}.json'; final=path.exists()
    if not final: path=gate/f'provisional/{task:03d}.json'
    r=json.loads(path.read_text()); assert r['rank']==14
    assert classify(r,final)['status'] in ('pending_validation','checked_SAT','checked_UNSAT')
    bind(path); oracles[r['arm']]=str(path.relative_to(root))
authority=root/'benchmarks/runs/20260911-7335-root-watermark-build/manifest.json'
b=json.loads(authority.read_text()); assert b['status']=='PASS'; bind(authority)
for rel,digest in b['files'].items(): bind(root/b['source']/rel,digest)
binary=root/'build/pin-7335-root-watermark-release/tactix'
bind(binary,b['binaries']['release'][str(binary.relative_to(root))])
for folder in ('pins','logs','claims','provisional','results'): (c/folder).mkdir(parents=True)
for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):
    shutil.copy2(gate/'pins'/name,c/'pins'/name)
arms={}
for name,old in [('parent','current_B'),('both','candidate_B')]:
    info=dict(m['arms'][old]); info['path']='pins/'+name
    shutil.copy2(gate/m['arms'][old]['path'],c/info['path']); arms[name]=info
arms['root_only']=dict(path='pins/root_only',sha256=sha(binary),arguments=[],
    source_commit=m['arms']['candidate_B']['source_commit'],
    source_manifest=str(authority.relative_to(root)),source_build=str(binary.parent.relative_to(root)))
shutil.copy2(binary,c/'pins/root_only')
rows=[dict(task=i,rank=14,arm=arm,tier='B',instance=r['instance'],expected=r['expected'])
      for i,arm in enumerate(('parent','root_only','both'))]
with (c/'matrix.tsv').open('x',newline='') as out:
    writer=csv.DictWriter(out,fieldnames=rows[0],delimiter='\t',lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch=(gate/'array.sbatch').read_text().replace(str(gate),str(c)).replace('7347-root-storage-BC','7348-root-collector-isolation').replace('--array=0-65','--array=0-2')
with (c/'array.sbatch').open('x') as out: out.write(batch)
for path in [Path(__file__),Path(__file__).with_name('check-submit-root-collector-isolation-7348.py')]: bind(path)
manifest=dict(schema='tactix-root-collector-isolation-v1',arms=arms,
    input_sha256={r['instance']:m['input_sha256'][r['instance']]},collect_runtime_context=True,
    resource_contract=dict(m['resource_contract'],array='0-2'),dependencies=dependencies,
    previous_work_oracles=oracles,
    hypothesis='BC7347 B14 is a changed-work regression, unlike storage-only counter differences on other completed pairs. Compare parent7302, root-only7335 and root+collector7340 to attribute the changed trajectory before any new solver edits or long timing waves.',
    acceptance='Three expected verdicts; independent checked original certificates and clean final state. Parent and both must exactly replay their own BC7347 work and certificates. Compare complete raw non-timing statistics/certificates for root_only versus both, enumerating every difference. No exclusions and no runtime acceptance claim from this diagnostic.',
    test_consideration='Existing immutable Release builds with fixed proof/model gates; new identity/arm-matrix negative controls. The inherited disconnected-watch defect is intentionally present only in the root_only diagnostic control; no promotion.')
manifest['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as out: json.dump(manifest,out,indent=2)
for path in (c/'pins').iterdir(): path.chmod(path.stat().st_mode & ~0o222)
print(c)
