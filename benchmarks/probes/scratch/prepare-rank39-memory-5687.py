#!/usr/bin/env python3
"""Attribute current Rank39 memory without changing solver behavior or allocation policy."""
import csv
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5664-propagation-cursor-ranks1-40'
authority = root / 'benchmarks/runs/20260908-5649-propagation-cursor-validated'
c = root / 'benchmarks/runs/20260908-5687-rank39-memory-ownership'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
score = json.loads((parent/'solver-checkpoint.json').read_text())
assert score['solver_records'] == 80
selected = score['per_rank']['39']['tactix']
assert sha(root/selected['record']) == selected['record_sha256']
m = json.loads((parent/'manifest.json').read_text())
a = json.loads((authority/'manifest.json').read_text())
trace = root/'build/pin-propagation-cursor-repair-5647-trace/tactix'
assert sha(trace) == a['binaries'][str(trace.relative_to(root))]
for name,digest in m['pin_sha256'].items(): assert sha(parent/name) == digest
c.mkdir(exist_ok=False)
for name in ('pins','logs','results','provisional','claims'): (c/name).mkdir()
for name in ('runtime_task.py','checkmodel','modelcheck.py','drat-trim'):
    shutil.copy2(parent/'pins'/name,c/'pins'/name)
shutil.copy2(trace,c/'pins/candidate')
shutil.copy2(authority/'cumulative.patch',c/'pins/candidate.patch')
shutil.copy2(root/'benchmarks/probes/scratch/memory-ownership-task-5386.py',c/'pins/run.py')
with (parent/'matrix.tsv').open(newline='') as stream:
    reader=csv.DictReader(stream,delimiter='\t');fields=reader.fieldnames
    rows=[r for r in reader if r['rank']=='39' and r['arm']=='tactix']
assert len(rows)==1
rows[0].update(task='0',arm='candidate')
with (c/'matrix.tsv').open('w',newline='') as stream:
    writer=csv.DictWriter(stream,fields,delimiter='\t',lineterminator='\n')
    writer.writeheader();writer.writerows(rows)
sbatch=(parent/'array.sbatch').read_text()
assert sbatch.count('--array=0-79')==1
sbatch=sbatch.replace(str(parent),str(c)).replace('--array=0-79','--array=0-0').replace('/pins/runtime_task.py','/pins/run.py')
(c/'array.sbatch').write_text(sbatch)
arm=dict(m['arms']['tactix'])
arm.update(path='pins/candidate',sha256=sha(c/'pins/candidate'),source_build=str(trace.parent.relative_to(root)),
    source_patch='pins/candidate.patch',source_patch_sha256=sha(c/'pins/candidate.patch'))
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),arms={'candidate':arm},
    input_sha256={rows[0]['instance']:m['input_sha256'][rows[0]['instance']]},
    candidate_authority=str(authority.relative_to(root)),parent_campaign=str(parent.relative_to(root)),
    parent_checkpoint_sha256=sha(parent/'solver-checkpoint.json'),release_record=selected,
    purpose='Attribute current5649 Rank39 memory ownership:Release peak2.679x native despite fewer reported propagations and1.162x runtime.',
    hypothesis='Persistent clause/watch backing, phase-local inprocessing storage or allocator retention may explain excess memory and contribute to unit cost. Identify owners before changing layout or release policy.',
    validation='Expected UNSAT outcome,complete non-timing statistics/proof-byte identity against full5664 Rank39,immutable trace hashes and strict row parser. Interpret provisional solver output while native proof checking continues;gate closes only checked/clean.',
    limitations='Read-only slot-memory observer with no broad event trace or stop. Diagnostic peak is not production acceptance timing/memory;rule-local scratch is not exhaustively inventoried. Native comparison remains the complete clean full5664 record.',
    test_consideration='Reuse unchanged tested memory observer and checked runtime wrapper. New strict ownership-row parser positive/negative fixtures plus exact Release work/proof replay validate this configuration-only use;no solver unit test change.',
    collect_runtime_context=True)
m['resource_contract']['array']='0-0'
m['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode&~0o222)
command=['sbatch','--parsable',str((c/'array.sbatch').relative_to(root))]
job=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True).stdout.strip()
assert job.isdecimal()
(c/'submission.json').write_text(json.dumps(dict(job=job,command=command,manifest_sha256=sha(c/'manifest.json')),indent=2)+'\n')
print('Submitted current Rank39 ownership probe',job)
