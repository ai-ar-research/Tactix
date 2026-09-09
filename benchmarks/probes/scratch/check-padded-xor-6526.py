#!/usr/bin/env python3
"""Fixed public-rule controls against immutable6508 objects, with final micro proofs."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6526-padded-xor-control'; assert not c.exists(); c.mkdir()
authority=root/'benchmarks/runs/20260909-6508-congruence-stages-build/manifest.json'
a=json.loads(authority.read_text()); source=root/a['source']; pin=root/'build/pin-6508-congruence-stages-release'
for rel,digest in a['files'].items(): assert sha(source/rel)==digest,rel
assert sha(pin/'tactix')==a['binaries'][str((pin/'tactix').relative_to(root))]
fixture=Path(__file__).with_name('test-padded-xor-6525.cpp')
objects=sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o')); assert objects
hashes={str(p.relative_to(root)):sha(p) for p in [authority,fixture,*objects]}
command=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),*map(str,objects),'-o',str(c/'fixture')]
with (c/'build.log').open('x') as log: subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
records=[]; removed=0
for mode in (0,1):
    for position in range(4):
        for missing in (0,1):
            base=c/f'{mode}-{position}-{missing}'; cnf=base.with_suffix('.cnf'); proof=base.with_suffix('.proof')
            command=[str(c/'fixture'),str(mode),str(position),str(missing),str(base),str(proof)]
            p=subprocess.run(command,capture_output=True,text=True)
            log=dict(command=command,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr)
            base.with_suffix('.json').write_text(json.dumps(log,indent=2)+'\n')
            assert p.returncode==0,(base,p.stderr)
            record=dict(mode=mode,position=position,missing=missing,fixture=log,input_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size)
            if not missing:
                checker=root/'tools/checkers/bin'/('lrat-check' if mode else 'drat-trim')
                command=[str(checker),str(cnf),str(proof)]
                q=subprocess.run(command,capture_output=True,text=True)
                record['checker']=dict(command=command,sha256=sha(checker),exit_code=q.returncode,stdout=q.stdout,stderr=q.stderr)
                assert q.returncode==0 and ('c VERIFIED' if mode else 's VERIFIED') in q.stdout,(base,q.stdout,q.stderr)
            records.append(record)
            removed+=cnf.stat().st_size+proof.stat().st_size
            cnf.unlink(); proof.unlink()
(c/'summary.json').write_text(json.dumps(dict(cases=len(records),final_proofs_checked=sum(not r['missing'] for r in records),
    records=records,hashes=hashes,fixture_binary_sha256=sha(c/'fixture'),removed_bytes=removed,
    limitation='Fixed five-variable public-rule tests, eight exhaustive32-assignment SAT controls and eight independently checked UNSAT proofs; not fuzz or runtime evidence. No source change or native omission copied.'),indent=2)+'\n')
print('PASS16 public-rule cases,8 checked final micro proofs; removed bytes',removed)
