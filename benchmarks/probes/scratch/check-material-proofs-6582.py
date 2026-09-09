#!/usr/bin/env python3
"""Fixed C2 round/budget/public-instance proofs and exhaustive model sets."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6582-material-proof-control'
resume=sys.argv[1:] in (['--resume-fixture'],['--resume-public-closure'])
assert sys.argv[1:] in ([],['--resume-fixture'],['--resume-public-closure'])
stage=3 if sys.argv[1:]==['--resume-public-closure'] else 2
assert c.is_dir() if resume else not c.exists()
assert not (c/'summary.json').exists()
authority=root/'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json'
a=json.loads(authority.read_text()); source=root/a['source']
pin=root/'build/pin-6579-material-schedule-release'
for rel,digest in a['files'].items(): assert sha(source/rel)==digest,rel
assert sha(pin/'tactix')==a['binaries'][str((pin/'tactix').relative_to(root))]
if not resume: c.mkdir()
fixture=Path(__file__).with_name('test-material-proofs-6581.cpp')
objects=sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o')); assert objects
hashes={str(p.relative_to(root)):sha(p) for p in [authority,fixture,*objects]}
binary=c/(f'fixture-{stage}' if resume else 'fixture')
assert not binary.exists()
command=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),*map(str,objects),'-o',str(binary)]
with (c/(f'build-{stage}.log' if resume else 'build.log')).open('x') as out:
    subprocess.run(command,stdout=out,stderr=subprocess.STDOUT,check=True)
records=[]; removed=0
for mode in (0,1):
    for n in (3,4):
        for missing in (0,1):
            for style in (0,1,2):
                base=c/f'{mode}-{n}-{missing}-{style}'
                completed=list(c.glob(base.name+'*.result.json')) if resume else []
                if completed:
                    assert len(completed)==1
                    record=json.loads(completed[0].read_text())
                    assert record['fixture']['exit_code']==0
                    if not missing: assert record['checker']['exit_code']==0
                    records.append(record)
                    continue
                if resume and (mode,n,missing,style)==(0,3,0,0):
                    # Already independently checked and cleaned before the
                    # later fixture failure. Do not rerun a successful case.
                    check=json.loads(base.with_suffix('.check.json').read_text())
                    assert check['exit_code']==0 and 's VERIFIED' in check['stdout']
                    records.append(dict(mode=mode,n=n,missing=missing,style=style,
                        fixture=json.loads(base.with_suffix('.json').read_text()),checker=check,
                        effects=json.loads(base.with_suffix('.effects.json').read_text()),
                        provenance='Original fixture retained as material-proof-initial-fixture-6583.cpp; original binary retained. Proof hash and removed-byte count were not persisted before the later assertion; no certificate retained and no rerun.'))
                    continue
                if resume and base.with_suffix('.json').exists():
                    assert json.loads(base.with_suffix('.json').read_text())['exit_code']!=0
                    base=c/(base.name+'-retry')
                cnf=base.with_suffix('.cnf'); proof=base.with_suffix('.proof')
                effects=base.with_suffix('.effects.json')
                command=[str(binary),str(mode),str(n),str(missing),str(style),str(cnf),str(proof),str(effects)]
                p=subprocess.run(command,capture_output=True,text=True)
                log=dict(command=command,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr)
                with base.with_suffix('.json').open('x') as out: json.dump(log,out,indent=2)
                assert p.returncode==0,(base,p.stderr)
                record=dict(mode=mode,n=n,missing=missing,style=style,fixture=log,
                    input_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size,
                    effects=json.loads(effects.read_text()))
                if style==1: assert record['effects']['budget_stops']>0
                if not missing:
                    checker=root/'tools/checkers/bin'/('lrat-check' if mode else 'drat-trim')
                    command=[str(checker),str(cnf),str(proof)]
                    q=subprocess.run(command,capture_output=True,text=True)
                    record['checker']=dict(command=command,sha256=sha(checker),exit_code=q.returncode,stdout=q.stdout,stderr=q.stderr)
                    with base.with_suffix('.check.json').open('x') as out: json.dump(record['checker'],out,indent=2)
                    assert q.returncode==0 and ('c VERIFIED' if mode else 's VERIFIED') in q.stdout,(base,q.stdout,q.stderr)
                records.append(record)
                with base.with_suffix('.result.json').open('x') as out: json.dump(record,out,indent=2)
                removed+=cnf.stat().st_size+proof.stat().st_size
                cnf.unlink(); proof.unlink()
cleanup=[]
if resume:
    for name in ('0-3-0-1.cnf','0-3-0-1.proof','diagnostic.cnf','diagnostic.proof','0-3-0-2.cnf','0-3-0-2.proof'):
        path=c/name
        cleanup.append(dict(path=name,sha256=sha(path),bytes=path.stat().st_size))
        removed+=path.stat().st_size; path.unlink()
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
with (c/'summary.json').open('x') as out:
    json.dump(dict(cases=len(records),final_proofs_checked=sum(not r['missing'] for r in records),
        records=records,hashes=hashes,fixture_binary_sha256=sha(binary),removed_bytes=removed,failed_fixture_cleanup=cleanup,
        limitation='24 fixed C2 round/budget/public-instance tests,12 exhaustive SAT model-set controls and12 independently checked final DRAT/LRAT micro proofs. Not fuzz or runtime evidence.'),out,indent=2)
print('PASS24 actual C2 cases,12 final checked proofs,12 exact model sets; removed bytes',removed)
