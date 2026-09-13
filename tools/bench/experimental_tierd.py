#!/usr/bin/env python3
"""Pinned experimental Tier D with anonymous local certificates and formulas."""
import argparse
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]

def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1<<20),b''):h.update(b)
    return h.hexdigest()

def save(p,data):
    with p.open('x') as f:json.dump(data,f,indent=2);f.write('\n')

def verify(c):
    m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['pin_sha256'].items():
        if sha(c/rel)!=digest:raise ValueError('pin mismatch: '+rel)
    return m

def prepare(a):
    c=a.campaign.resolve(); ref=ROOT/'benchmarks/baselines/sc24-26-32g-5000s'
    revision=a.binary_source_commit or 'HEAD'
    commit=subprocess.check_output(['git','rev-parse',revision],cwd=ROOT,text=True).strip()
    # Every archived source/build input must be exactly the committed blob.
    source=a.source.resolve()
    listing=subprocess.check_output(['git','ls-tree','-rz',commit,'--','src','include','CMakeLists.txt'],cwd=ROOT)
    sources={}
    for entry in listing.split(b'\0'):
        if not entry:continue
        metadata,name=entry.split(b'\t',1);mode,kind,oid=metadata.split();name=name.decode()
        p=source/name
        if kind!=b'blob' or not p.is_file():continue
        actual=subprocess.check_output(['git','hash-object',str(p)],cwd=ROOT).strip()
        if actual!=oid:raise ValueError('source differs from HEAD: '+name)
        sources[name]=sha(p)
    if not all(str(p.relative_to(source)) in sources for p in (source/'src').rglob('*') if p.is_file()):
        raise ValueError('uncommitted source in build tree')
    manifest=json.loads((ref/'manifest.json').read_text())
    assert sha(ref/'manifest.json')==json.loads((ref/'launch.json').read_text())['manifest_sha256']
    assert sha(ref/'cache.sha256')==manifest['cache_manifest_sha256']
    inputs=dict((rel,digest) for digest,rel in (line.split() for line in (ref/'cache.sha256').read_text().splitlines()))
    source_rows=[dict(rank=i+1,arm='tactix',tier='D',instance=r['rel'],expected=r['expected']) for i,r in enumerate(manifest['instances'])]
    assert len(source_rows)==1166 and len({r['instance'] for r in source_rows})==1166
    if a.instance:
        selected=set(a.instance)
        if len(selected)!=len(a.instance):raise ValueError('duplicate --instance')
        known={r['instance'] for r in source_rows}
        missing=selected-known
        if missing:raise ValueError('unknown Tier-D instance: '+sorted(missing)[0])
        source_rows=[r for r in source_rows if r['instance'] in selected]
    rows=[dict(task=i,**r) for i,r in enumerate(source_rows)]
    assert rows and len({r['instance'] for r in rows})==len(rows)
    inputs={rel:digest for rel,digest in inputs.items() if rel in {r['instance'] for r in rows}}
    assert set(inputs)=={r['instance'] for r in rows}
    assert all((ROOT/'benchmarks/cache'/rel).is_file() for rel in inputs)
    c.mkdir()
    for name in ['pins','logs','claims','provisional','results']:(c/name).mkdir()
    shutil.copy2(a.binary,c/'pins/tactix')
    for name in ['checkmodel','modelcheck.py','drat-trim']:
        shutil.copy2(ref/'pins'/name,c/'pins'/name)
    for name in ['runtime_task.py','proof_oracle.py','anonymous_scratch.py','test_anonymous_scratch.py','validate_anonymous_runtime.py']:
        shutil.copy2(ROOT/'tools/bench'/name,c/'pins'/name)
    with (c/'matrix.tsv').open('x',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]),delimiter='\t');w.writeheader();w.writerows(rows)
    common=f'''#!/usr/bin/env bash
#SBATCH --partition={a.partition}
#SBATCH --ntasks=1
#SBATCH --cpus-per-task={a.cpus_per_task}
#SBATCH --mem=32768M
#SBATCH --no-requeue
#SBATCH --output={c}/logs/%x-%A_%a.log
'''
    (c/'array.sbatch').write_text(common+f'''#SBATCH --job-name=tierd-{commit[:8]}
#SBATCH --time=05:00:00
#SBATCH --array=0-{len(rows)-1}
set -euo pipefail
ulimit -c 0
exec python3 {c}/pins/runtime_task.py {c} "$SLURM_ARRAY_TASK_ID"
''')
    (c/'validate.sbatch').write_text(common+f'''#SBATCH --job-name=tierd-anonymous-check
#SBATCH --time=00:05:00
set -euo pipefail
ulimit -c 0
python3 {c}/pins/test_anonymous_scratch.py
exec python3 {c}/pins/validate_anonymous_runtime.py {c}
''')
    m=dict(schema='tactix-experimental-single-tierd-v3',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        source_commit=commit,source_tree=str(source),source_files=sources,
        arms={'tactix':dict(path='pins/tactix',sha256=sha(c/'pins/tactix'),arguments=[])},
        input_sha256=inputs,scratch_policy='anonymous-local-files-v1',
        dataset_authority=dict(path=str(ref/'manifest.json'),sha256=sha(ref/'manifest.json')),
        resource_contract=dict(partition=a.partition,cpus_per_task=a.cpus_per_task,memory_mib=32768,solve_timeout=1200,
            checker_timeout=16000,scheduler_wall_seconds=18000,array=f'0-{len(rows)-1}',throttle=None,node_constraint=None),
        cleanup_contract='O_TMPFILE on /tmp for input, proof, model and checker scratch; no directory entries ever. Kernel releases storage at last close, including SIGKILL/OOM. Fail closed if unsupported. Only bounded JSON metadata and scheduler logs persist; core dumps disabled.',
        caveat='If the entire job dies, its result may be missing; never interpret missing as timeout or checked. Local disk may fill during a live run; no home-quota certificate storage or failure retention.',
        pin_sha256={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch',c/'validate.sbatch']})
    save(c/'manifest.json',m)
    for p in [*(c/'pins').iterdir(),c/'manifest.json',c/'matrix.tsv',c/'array.sbatch',c/'validate.sbatch']:p.chmod(p.stat().st_mode&~0o222)
    verify(c); print(c)

def submit(a):
    c=a.campaign.resolve();verify(c)
    name='validation-launch.json' if a.validate else 'launch.json'
    assert not (c/name).exists()
    if not a.validate:assert json.loads((c/'validation.json').read_text())['passed']
    script=c/('validate.sbatch' if a.validate else 'array.sbatch')
    job=subprocess.check_output(['sbatch','--parsable',str(script)],text=True).strip().split(';')[0]
    save(c/name,dict(job_id=job,manifest_sha256=sha(c/'manifest.json'),submitted_at=dt.datetime.now(dt.timezone.utc).isoformat()))
    print(job)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--campaign',type=Path,required=True)
    p.add_argument('--source',type=Path)
    p.add_argument('--binary',type=Path)
    p.add_argument('--binary-source-commit')
    p.add_argument('--partition',default='cpu-q')
    p.add_argument('--cpus-per-task',type=int,default=4)
    p.add_argument('--instance',action='append',default=[])
    g=p.add_mutually_exclusive_group(required=True)
    g.add_argument('--prepare',action='store_true');g.add_argument('--validate',action='store_true');g.add_argument('--submit',action='store_true')
    a=p.parse_args()
    if a.cpus_per_task < 1:p.error('--cpus-per-task must be positive')
    prepare(a) if a.prepare else submit(a)
