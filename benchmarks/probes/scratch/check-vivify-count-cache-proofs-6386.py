#!/usr/bin/env python3
"""Independent DRAT/LRAT checking of fixed canonical transactions."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6386-vivify-count-cache-proofs'
assert not c.exists(); c.mkdir()
source=root/'.claude/worktrees/runtime-vivify-count-cache-6383'
pin=root/'build/pin-6383-vivify-count-cache-release'
fixture=root/'benchmarks/probes/scratch/test-vivify-count-cache-proof-6386.cpp'
assert (root/'benchmarks/runs/20260909-6383-vivify-count-cache-build/manifest.json').exists()
objects=sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
command=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),*map(str,objects),'-o',str(c/'fixture')]
with (c/'build.log').open('x') as log: subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
records=[]; removed=0
for scope in range(6):
    for reuse in (0,1):
        for flush in (0,1):
            for mode in (0,1):
                name=f'{scope}-{reuse}-{flush}-{mode}'; cnf=c/(name+'.cnf'); proof=c/(name+'.proof')
                invocation=[str(c/'fixture'),str(scope),str(reuse),str(flush),str(mode),str(cnf),str(proof)]
                p=subprocess.run(invocation,cwd=c,capture_output=True,text=True)
                (c/(name+'-fixture.json')).write_text(json.dumps(dict(command=invocation,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr),indent=2)+'\n')
                assert p.returncode==0
                checker=root/('tools/checkers/bin/lrat-check' if mode else 'tools/checkers/bin/drat-trim')
                command=[str(checker),str(cnf),str(proof)]
                q=subprocess.run(command,capture_output=True,text=True)
                assert q.returncode==0 and ('c VERIFIED' if mode else 's VERIFIED') in q.stdout,(name,q.stdout,q.stderr)
                records.append(dict(name=name,command=command,checker_sha256=sha(checker),stdout=q.stdout,stderr=q.stderr,
                    fixture_stdout=p.stdout,input_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size))
                removed+=cnf.stat().st_size+proof.stat().st_size
                proof.unlink(); cnf.unlink()
(c/'summary.json').write_text(json.dumps(dict(checked=len(records),records=records,fixture_sha256=sha(fixture),
    binary_sha256=sha(c/'fixture'),removed_bytes=removed,
    limitation='Fixed canonical proof coverage with proof-backed duplicates ensuring candidates in every scope;432 unit cases separately cover model sets. Not current-source fuzz or a broad certificate gate.'),indent=2)+'\n')
print('PASS:',len(records),'independent final proofs; removed bytes',removed)
