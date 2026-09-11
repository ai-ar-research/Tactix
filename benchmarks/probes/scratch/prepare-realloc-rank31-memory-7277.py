#!/usr/bin/env python3
"""Diagnose the observed Rank31 RSS increase without changing either solver."""
import csv
import datetime as dt
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

def wrapper_fixtures():
    spec=importlib.util.spec_from_file_location('memory_observer',root/'tools/bench/slot_memory_task.py')
    observer=importlib.util.module_from_spec(spec); spec.loader.exec_module(observer)
    before=os.environ.get(observer.VARIABLE)
    try:
        for selected in ('previous','candidate','checker'):
            with tempfile.TemporaryDirectory(dir=root/'benchmarks/probes/scratch') as tmp:
                c=Path(tmp); (c/'logs').mkdir(); seen=[]
                os.environ.pop(observer.VARIABLE,None)
                def measured(command,*args):
                    value=os.environ.get(observer.VARIABLE); seen.append(value)
                    if value: Path(value).write_text('fixed observer fixture\n')
                    return {'exit_code':0}
                protocol=SimpleNamespace(measured=measured,sha256=sha)
                for arm in ('previous','candidate'):
                    observer.install_observer(protocol,c,0,c/arm)
                result=protocol.measured([str(c/selected)],None,None,None,1)
                assert len(seen)==1 and (seen[0] is not None)==(selected!='checker')
                assert ('memory_trace' in result)==(selected!='checker')
                assert observer.VARIABLE not in os.environ
    finally:
        if before is None: os.environ.pop(observer.VARIABLE,None)
        else: os.environ[observer.VARIABLE]=before
    print('Paired memory wrapper: both solver aliases, non-solver bypass, single invocation and environment restoration PASS')

def main():
    assert sys.argv[1:] in ([],['--self-test'])
    wrapper_fixtures()
    if sys.argv[1:]: return
    c=root/'benchmarks/runs/20260911-7277-realloc-rank31-memory'
    assert not c.exists(),'preserve prepared/submitted evidence'
    runtime=root/'benchmarks/runs/20260911-7273-dense-realloc-runtime'
    m=json.loads((runtime/'manifest.json').read_text()); evidence={}
    def bind(p,expected=None):
        value=sha(p); assert expected is None or value==expected,p
        evidence[str(p.relative_to(root))]=value
    bind(runtime/'manifest.json')
    for rel,digest in m['pin_sha256'].items(): bind(runtime/rel,digest)
    oracles={}; builds={}
    for task,arm,stem in ((4,'candidate','7271-dense-realloc'),(5,'previous','7253-occurrence-cutover')):
        p=runtime/f'results/{task:03d}.json'; final=p.exists()
        if not final: p=runtime/f'provisional/{task:03d}.json'
        r=json.loads(p.read_text()); bind(p)
        assert (r['rank'],r['arm'])==(31,arm)
        assert classify(r,final)['status'] in ('pending_validation','checked_UNSAT')
        oracles[arm]=dict(path=str(p.relative_to(root)),sha256=sha(p))
        authority=root/f'benchmarks/runs/20260911-{stem}-build/manifest.json'
        b=json.loads(authority.read_text()); bind(authority)
        for rel,digest in b['files'].items(): bind(root/b['source']/rel,digest)
        builds[arm]=b
    for name in ('pins','logs','claims','provisional','results'): (c/name).mkdir(parents=True)
    for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):
        shutil.copy2(runtime/'pins'/name,c/'pins'/name)
    wrapper=root/'tools/bench/slot_memory_task.py'; bind(wrapper)
    code=wrapper.read_text()
    old="    install_observer(protocol, campaign, sys.argv[2], campaign / 'pins/tactix')"
    assert code.count(old)==1
    code=code.replace(old,"    for arm in ('previous', 'candidate'):\n        install_observer(protocol, campaign, sys.argv[2], campaign / 'pins' / arm)")
    with (c/'pins/run.py').open('x') as out: out.write(code)
    arms={}; rows=[]
    for arm,stem in (('candidate','7267-dense-realloc'),('previous','7253-occurrence-cutover')):
        binary=root/f'build/pin-{stem}-trace/tactix'
        bind(binary,builds[arm]['binaries'][str(binary.relative_to(root))])
        shutil.copy2(binary,c/'pins'/arm)
        arms[arm]=dict(path='pins/'+arm,sha256=sha(binary),arguments=[],
            source_commit=builds[arm]['source_commit'],source_build=str(binary.parent.relative_to(root)))
        r=json.loads((root/oracles[arm]['path']).read_text())
        rows.append(dict(task=len(rows),rank=31,arm=arm,tier='D',instance=r['instance'],expected='UNSAT'))
    assert rows[0]['instance']==rows[1]['instance']
    with (c/'matrix.tsv').open('x',newline='') as out:
        w=csv.DictWriter(out,rows[0],delimiter='\t',lineterminator='\n'); w.writeheader(); w.writerows(rows)
    batch=(runtime/'array.sbatch').read_text().replace(str(runtime),str(c))
    batch=batch.replace('7273-dense-realloc-runtime','7277-realloc-rank31-memory').replace('--array=0-29','--array=0-1').replace('/pins/runtime_task.py','/pins/run.py')
    with (c/'array.sbatch').open('x') as out: out.write(batch)
    bind(Path(__file__)); bind(Path(__file__).with_name('validate-realloc-rank31-memory-7277.py'))
    manifest=dict(schema='tactix-realloc-rank31-memory-v1',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        arms=arms,release_records=oracles,evidence_sha256=evidence,collect_runtime_context=True,
        input_sha256={rows[0]['instance']:m['input_sha256'][rows[0]['instance']]},
        resource_contract=dict(m['resource_contract'],array='0-1'),
        hypothesis='Rank31 wave0 realloc peak562920KiB versus parent495156KiB despite exact work/capacities: locate first peak and active owners before assuming the growth change uniformly saves memory.',
        acceptance='Whole own Release work/certificate fingerprint including every physical counter exact. Existing read-only observer only; independently checked original proofs and clean final records required. Pending proofs permit diagnosis, not correctness acceptance.',
        test_consideration='No solver change. Existing six observer tests plus new both-alias/non-solver/single-invocation/environment-restoration controls; strict current trace and collector parsing reused.',
        limitation='Trace timings/RSS are diagnostic, not runtime acceptance. Distinct allocator history and unsampled transients may change peaks; no forced stop, scheduling override, or Rank21 reopening.')
    manifest['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
    with (c/'manifest.json').open('x') as out: json.dump(manifest,out,indent=2)
    for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)

if __name__=='__main__': main()
