#!/usr/bin/env python3
"""Pin current read-only memory ownership on the two unshelved dense witnesses."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root/'benchmarks/runs/20260911-7265-occurrence-memory'
assert not c.exists(), 'do not overwrite prepared or submitted evidence'
old = root/'benchmarks/runs/20260911-7262-occurrence-ranks1-50'
subprocess.run([sys.executable,str(Path(__file__).with_name('validate-occurrence-full50-7262.py'))],check=True)
evidence = {}
def bind(p,expected=None):
    digest = sha(p); assert expected is None or digest == expected,p
    evidence[str(p.relative_to(root))] = digest
m = json.loads((old/'manifest.json').read_text()); bind(old/'manifest.json')
for rel,digest in m['pin_sha256'].items(): bind(old/rel,digest)
build = root/'benchmarks/runs/20260911-7253-occurrence-cutover-build'
b = json.loads((build/'manifest.json').read_text()); bind(build/'manifest.json')
assert b['status'] == 'PASS'
for rel,digest in b['files'].items(): bind(root/b['source']/rel,digest)
for rel,digest in b['binaries'].items(): bind(root/rel,digest)
for kind,count in (('release',117),('bc',117),('trace',116)):
    p = build/f'logs/{kind}-ctest.log'; bind(p)
    assert f'100% tests passed out of {count}' in p.read_text()
release_records = {}
for rank in (9,25):
    p = old/f'results/{rank-1:03d}.json'; final = p.exists()
    if not final: p = old/f'provisional/{rank-1:03d}.json'
    r = json.loads(p.read_text()); bind(p)
    assert r['rank'] == rank and r['verdict'] == r['expected'] == 'SAT'
    assert classify(r,final)['status'] in ('checked_SAT','pending_validation')
    release_records[str(rank)] = dict(path=str(p.relative_to(root)),sha256=sha(p))
for p in (root/'tools/bench/slot_memory_task.py',root/'tools/bench/test_slot_memory_task.py',Path(__file__)):
    bind(p)
subprocess.run([sys.executable,'-m','unittest','discover','-s','tools/bench','-p','test_slot_memory_task.py'],cwd=root,check=True)
for name in ('pins','logs','results','provisional','claims'): (c/name).mkdir(parents=True)
for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):
    shutil.copy2(old/'pins'/name,c/'pins'/name)
shutil.copy2(root/'tools/bench/slot_memory_task.py',c/'pins/run.py')
binary = root/'build/pin-7253-occurrence-cutover-trace/tactix'
shutil.copy2(binary,c/'pins/tactix')
with (old/'matrix.tsv').open() as stream:
    reader = csv.DictReader(stream,delimiter='\t'); fields = reader.fieldnames
    rows = [r for r in reader if int(r['rank']) in (9,25)]
assert [int(r['rank']) for r in rows] == [9,25]
for task,row in enumerate(rows): row['task'] = str(task)
with (c/'matrix.tsv').open('x',newline='') as stream:
    writer = csv.DictWriter(stream,fields,delimiter='\t',lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (old/'array.sbatch').read_text().replace(str(old),str(c))
assert '--array=0-49' in batch and '/pins/runtime_task.py' in batch
batch = batch.replace('--array=0-49','--array=0-1').replace('/pins/runtime_task.py','/pins/run.py')
with (c/'array.sbatch').open('x') as stream: stream.write(batch)
arm = dict(path='pins/tactix',sha256=sha(binary),source_build=str(binary.parent.relative_to(root)),
    source_commit=m['arms']['tactix']['source_commit'],arguments=[])
manifest = dict(schema='tactix-current-occurrence-memory-ownership-v1',
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),arms={'tactix':arm},
    input_sha256={r['instance']:m['input_sha256'][r['instance']] for r in rows},
    collect_runtime_context=True,resource_contract=dict(m['resource_contract'],array='0-1'),
    release_records=release_records,evidence_sha256=evidence,
    solve_timeout=1200,checker_timeout=16000,
    hypothesis='Locate remaining dense-memory peaks on current7253 Ranks9/25, the two largest unshelved dense-memory witnesses. Distinguish capacity-growth intervals from retained formula, reconstruction, proof and allocator ownership after removing the duplicate image.',
    decision='Exact whole own7253 Release work and certificate fingerprint including physical dense counters, independently checked original models and clean scratch. Read-only attribution is not Release timing/RSS acceptance, an allocation-level causal proof, or a reason to reopen shelved21.',
    test_consideration='No new solver or observer source. Existing trace full116 suite includes memory/collection fixtures; existing six observer-wrapper tests cover solver-only environment/restoration, failure preservation and evidence ownership. New7266 reader fixtures validate peak selection and capacity interval framing with zero counts and malformed inputs.',
    limitations='Only existing TACTIX_SLOT_MEMORY_TRACE selected, no global events, forced stop or policy override. Trace timing/peak memory may differ from Release; unsampled transients and owners remain possible. No old6156 capacity replay is required across changed implementations, and no old observation is treated as current causality.')
manifest['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as stream: json.dump(manifest,stream,indent=2)
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
