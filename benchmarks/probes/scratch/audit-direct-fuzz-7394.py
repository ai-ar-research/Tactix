#!/usr/bin/env python3
"""One-shot direct-fuzz checkpoint; hash only terminal logs/published records."""
import datetime as dt
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
C=ROOT/'benchmarks/runs/20260911-7391-direct-root-fuzz'
S=ROOT/'benchmarks/runs/20260911-7393-root-fold-fuzz'
TARGET=ROOT/'benchmarks/reports/2026-09-11-direct-fuzz-audit-7394.json'
assert not TARGET.exists(), 'successful checkpoint is frozen'
hashes={}; owned=set()
def bind(p,expected=None,own=False):
    digest=sha(p);assert expected is None or digest==expected,str(p)
    rel=str(p.relative_to(ROOT));hashes[rel]=digest
    if own:owned.add(rel)
def read(p,own=False):bind(p,own=own);return json.loads(p.read_text())
def module(name):
    p=Path(__file__).with_name(name+'.py');bind(p,own=True)
    spec=importlib.util.spec_from_file_location(name,p);h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h);return h
h=module('fuzz-log-contract-7394');h.fixtures()
m=read(C/'manifest.json',True)
for rel,digest in m['hashes'].items():bind(ROOT/rel,digest)
for p in (C/'outcomes').glob('*.json'):bind(p,own=True)
for name in ('selftest','debug-build'):
    r=read(C/f'outcomes/{name}.json');assert r['status']=='PASS'
    bind(C/'logs'/('selftest.log' if name=='selftest' else 'debug-build.log'),r['log_sha256'],True)
legs={}; jobs=[]
for name,spec in m['legs'].items():
    launch=read(C/f'outcomes/launch-{name}.json');assert launch['exit_code']==0
    match=re.search(r'^submitted: (\d+)$',launch['stdout'],re.M);assert match
    job=match[1];jobs.append(job)
    result=subprocess.run(['sacct','-n','-X','-j',job,'-S',launch['accounting_since'],'-o','JobID,State%30,ExitCode,Elapsed,AllocCPUS','-P'],text=True,capture_output=True,check=True)
    rows=[row.split('|') for row in result.stdout.splitlines() if row.split('|')[0]==job];assert len(rows)==1
    state=rows[0][1].strip(); assert state in ('PENDING','RUNNING','COMPLETED'),(job,result.stdout)
    log=ROOT/f'tools/fuzz/slurm-7391-{name}-{job}.log'
    entry=dict(job_id=job,state=state,accounting=result.stdout,log=str(log.relative_to(ROOT)))
    if state=='COMPLETED':
        assert rows[0][2]=='0:0';body=log.read_text();entry['coverage']=h.parse(body,spec);bind(log,own=True)
        pin=re.search(r'^harness: ([0-9a-f]{64}) \((.*)\)$',launch['stdout'],re.M);assert pin
        assert 'harness-sha256='+pin[1] in body
        path=Path(pin[2]);assert path.parent==ROOT/'build' and path.name.startswith('pin-fuzz-harness-')
        assert not path.exists(), 'successful harness snapshot cleanup pending'
        entry['harness_snapshot_removed']=True
        nonterm=C/f'failures/{name}/nonterminating.json'
        if entry['coverage']['lost_coverage']:
            data=read(nonterm,True);assert data['nonterminating']==entry['coverage']['lost_coverage']
            assert data['cases_run']==10000 and data['timeout']==spec['timeout']
            for p in (C/f'failures/{name}/nonterminating').rglob('*'):
                if p.is_file():bind(p,own=True)
        assert all(p.name in ('nonterminating','nonterminating.json') for p in (C/f'failures/{name}').iterdir()), 'failure bundle requires diagnosis'
    elif log.exists():
        body=log.read_text()
        assert not re.search(r'^fuzz: \d+ cases, [1-9]\d* failures',body,re.M),'published fuzz failure'
        entry['limitation']='Live log not hashed or accepted as terminal.'
    legs[name]=entry
sm=read(S/'manifest.json',True);sl=read(S/'launch.json',True);jobs.append(sl['job_id'])
for rel,digest in sm['dependencies'].items():bind(ROOT/rel,digest)
for rel,digest in sm['pin_sha256'].items():
    p=S/rel;bind(p,digest,own=p.suffix in ('.py','.sbatch'))
supplement=dict(job_id=sl['job_id'],status='PENDING_VALIDATION')
if (S/'result.json').exists():
    r=read(S/'result.json',True);assert r['status']=='PASS' and r['completed']==10000 and r['failures']==0
    assert len(r['records'])==10000 and [row['index'] for row in r['records']]==list(range(10000))
    groups={}
    for row in r['records']:
        assert not row['failures'] and row['archived']>=2 and row['verdict']==row['expected']
        key=row['mode']+'/'+row['expected'];groups[key]=groups.get(key,0)+1
    assert groups=={'drat/SAT':2500,'drat/UNSAT':2500,'lrat/SAT':2500,'lrat/UNSAT':2500}
    supplement.update(status='SOLVER_CHECKS_PASS',strata=groups,physical_cores=r['available_physical_cores'],workers=r['requested_workers'])
campaigns={}
for stem,expected in [('7385-corrected-root-ranks1-50',50),('7382-corrected-root-runtime',12),('7389-current-gap-profiles',4)]:
    c=ROOT/('benchmarks/runs/20260911-'+stem);cm=read(c/'manifest.json',True);states={}
    for rel,digest in cm['pin_sha256'].items():
        p=c/rel;bind(p,digest,own=p.suffix in ('.py','.patch','.tsv','.sbatch'))
    for p in c.glob('launch*.json'):
        launch=read(p,True);jobs.append(str(launch['job_id']))
    for folder in ('provisional','results'):
        for p in (c/folder).glob('*.json'):
            r=read(p,True);state=classify(r,folder=='results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
            states[r['task']]=state
    campaigns[stem]=dict(expected=expected,published=len(states),checked=sum(s['checked'] for s in states.values()),states=states)
for name in ('current-gap-profiles-7389.py','prepare-direct-fuzz-7391.py','root-fold-fuzz-7393.py',
             'root-fold-fuzz-solver-7393.py','prepare-root-fold-fuzz-7393.py','audit-direct-fuzz-7394.py'):
    bind(Path(__file__).with_name(name),own=True)
queue=subprocess.run(['squeue','-h','-r','-j',','.join(sorted(set(jobs))),'-o','%i|%T|%C|%R'],text=True,capture_output=True,check=True)
with TARGET.open('x') as out:json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,
    owned_paths=sorted(owned),broad_legs=legs,root_supplement=supplement,campaigns=campaigns,queue=queue.stdout,
    new_manual_removed_bytes=0,
    limitation='Checkpoint only. Current-source fuzz requires all four broad legs plus direct lifecycle coverage and terminal cleanup. Do not count timeouts or crash-only corpus checks as verified SAT/UNSAT cases. Production/fixed score unchanged; no denied external transfer/push retried.'),out,indent=2)
print(TARGET)
print({name:row['state'] for name,row in legs.items()},supplement,flush=True)
