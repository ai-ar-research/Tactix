#!/usr/bin/env python3
"""Randomized original-CNF oracle checks through the existing root-fold fixture."""
import concurrent.futures
import importlib.util
import itertools
import json
import os
from pathlib import Path
import random
import re
import sys
import tempfile
import threading


def formula(seed):
    rng=random.Random(seed); roots=rng.randint(3,257); live=rng.randint(6,70)
    n=roots+live; ids=list(range(1,n+1)); rng.shuffle(ids)
    signs={v:rng.choice((-1,1)) for v in ids}
    def lit(v,positive=True): return ids[v]*signs[ids[v]]*(1 if positive else -1)
    rows=[[lit(v)] for v in range(roots)]
    for _ in range(rng.randint(live,4*live)):
        vs=rng.sample(range(roots,n),rng.randint(2,min(6,live)))
        choices=[rng.choice((False,True)) for _ in vs]
        if seed%2==0: choices[rng.randrange(len(vs))]=True
        rows.append([lit(v,p) for v,p in zip(vs,choices)])
    php=[]
    if seed%2:
        def cell(p,h): return lit(roots+2*p+h)
        php=[[cell(p,0),cell(p,1)] for p in range(3)]
        php += [[-cell(p,h),-cell(q,h)] for h in range(2) for p in range(3) for q in range(p+1,3)]
        rows+=php
    rng.shuffle(rows)
    return n,rows,roots,signs,php


def reached(stdout):
    matches=re.findall(r'^c fixed-root fixture archived (\d+)$',stdout,re.M)
    assert len(matches)==1 and int(matches[0])>=2, 'physical root folding not reached'
    return int(matches[0])


def fixtures():
    for seed in range(200):
        n,rows,roots,signs,php=formula(seed)
        units=[r[0] for r in rows if len(r)==1]
        assert len(units)==roots and len(set(map(abs,units)))==roots
        nonunit=[r for r in rows if len(r)>1]
        assert set(map(abs,units)).isdisjoint(abs(v) for r in nonunit for v in r)
        assert all(1<=abs(v)<=n for r in rows for v in r)
        if not seed%2:
            assert all(any(signs[abs(v)]==(1 if v>0 else -1) for v in r) for r in rows)
        else:
            variables=sorted({abs(v) for r in php for v in r}); assert len(variables)==6
            assert not any(all(any(values[variables.index(abs(v))]==(v>0) for v in r) for r in php)
                           for values in itertools.product((False,True),repeat=6))
    assert reached('c fixed-root fixture archived 2\n')==2
    for bad in ('','c fixed-root fixture archived 0\n','c fixed-root fixture archived 1\n',
                'c fixed-root fixture archived 2\nc fixed-root fixture archived 3\n'):
        try: reached(bad)
        except AssertionError: pass
        else: raise AssertionError('missing/ambiguous reachability accepted')
    print('Root generator: 200 SAT-witness/UNSAT-core/isolated-root checks and four negative reachability controls PASS',flush=True)


def main():
    fixtures()
    if sys.argv[1:]==['--self-test']: return
    assert os.environ.get('SLURM_JOB_ID') and len(sys.argv)==2
    c=Path(sys.argv[1]).resolve(); m=json.loads((c/'manifest.json').read_text())
    os.environ['TACTIX_REPO_ROOT']=str(c/'pins')
    sys.path.insert(0,str(c/'pins'))
    import fuzz as h
    import hashlib
    def sha(p):
        digest=hashlib.sha256()
        with p.open('rb') as stream:
            for block in iter(lambda:stream.read(8<<20),b''): digest.update(block)
        return digest.hexdigest()
    for rel,digest in m['pin_sha256'].items(): assert sha(c/rel)==digest,rel
    workers=50; assert int(os.environ['SLURM_CPUS_PER_TASK'])==100
    cpus=os.sched_getaffinity(0); assert len(cpus)>=100
    physical=set()
    for cpu in cpus:
        base=Path(f'/sys/devices/system/cpu/cpu{cpu}/topology')
        physical.add(((base/'physical_package_id').read_text().strip(),(base/'core_id').read_text().strip()))
    assert len(physical)>=workers, 'fewer physical cores than workers'
    stop=threading.Event(); lock=threading.Lock(); records=[]
    def run(i):
        if stop.is_set(): return
        seed=m['seed']+i; n,rows,roots,_,_=formula(seed); mode='drat' if i%4<2 else 'lrat'
        expected='SAT' if seed%2==0 else 'UNSAT'
        with tempfile.TemporaryDirectory(prefix='root-fold-fuzz-') as directory:
            work=Path(directory); cnf=work/'input.cnf'
            cnf.write_text(f'p cnf {n} {len(rows)}\n'+''.join(' '.join(map(str,r))+' 0\n' for r in rows))
            solver=[sys.executable,str(c/'pins/solver.py')]+(['--lrat'] if mode=='lrat' else [])
            oracle=[str(c/'pins/kissat')]
            result=h.run_case(str(cnf),solver,oracle,20,False,str(work))
            if result.verdict!=expected: result.fail('verdict',f'constructed expected {expected}, observed {result.verdict}')
            try: archived=reached(result.solver_out)
            except AssertionError as error: archived=0; result.fail('reachability',str(error))
            if result.oracle_verdict!=expected: result.fail('differential',f'reference expected {expected}, observed {result.oracle_verdict}')
            if result.failures:
                stop.set()
                h.save_bundle(str(c/'failures'),i,seed,'isolated-root-fold',str(cnf),result,solver,oracle,20,False,False)
            row=dict(index=i,seed=seed,mode=mode,expected=expected,verdict=result.verdict,
                roots=roots,archived=archived,failures=result.failures,cnf_sha256=sha(cnf))
            with lock:
                records.append(row)
                if result.failures or len(records)%100==0: print('Completed',len(records),'/',m['cases'],'failures',sum(bool(r['failures']) for r in records),flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        list(pool.map(run,range(m['cases'])))
    complete=len(records)==m['cases']; failures=sum(bool(r['failures']) for r in records)
    status='PASS' if complete and not failures else 'FAIL'
    output=dict(status=status,records=sorted(records,key=lambda r:r['index']),completed=len(records),failures=failures,
        requested_workers=workers,available_physical_cores=len(physical),affinity=sorted(cpus),
        meaning='Every counted success checked original-CNF model or native proof, reference verdict and >=2 actually archived roots. Each case scratch removed after verification or failure-bundle preservation.')
    with (c/'result.json').open('x') as out: json.dump(output,out,indent=2)
    print(status,len(records),'original-CNF checks,',failures,'failures',flush=True)
    raise SystemExit(0 if status=='PASS' else 1)


if __name__=='__main__': main()
