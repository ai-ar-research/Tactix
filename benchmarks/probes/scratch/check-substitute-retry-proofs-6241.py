#!/usr/bin/env python3
"""Independent proof oracles for fixed rule fixtures; no benchmark runs."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6241-substitute-retry-fixed-proofs'
assert not c.exists(); c.mkdir()
source=root/'.claude/worktrees/runtime-substitute-retry-6231'
pin=root/'build/pin-6231-substitute-retry-release'
fixture=root/'benchmarks/probes/scratch/test-substitute-retry-proof-model-6241.cpp'
command=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),
         *map(str,sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))),'-o',str(c/'fixture')]
p=subprocess.run(command,capture_output=True,text=True)
(c/'build.log').write_text(p.stdout+p.stderr); assert p.returncode==0
records=[]
for copies in (1,2,5):
    for mode in (0,1):
        for image in (False,True):
            name=f'{copies}-{mode}-{int(image)}'
            cnf=c/(name+'.cnf'); proof=c/(name+'.proof')
            command=['env']+(['TACTIX_SUBSTITUTE_IMAGE_PATH=1'] if image else ['-u','TACTIX_SUBSTITUTE_IMAGE_PATH'])+[str(c/'fixture'),str(copies),str(mode),str(cnf),str(proof)]
            p=subprocess.run(command,cwd=c,capture_output=True,text=True)
            (c/(name+'-fixture.log')).write_text(p.stdout+p.stderr); assert p.returncode==0
            checker=root/('tools/checkers/bin/lrat-check' if mode else 'tools/checkers/bin/drat-trim')
            command=[str(checker),str(cnf),str(proof)]
            p=subprocess.run(command,capture_output=True,text=True)
            (c/(name+'-check.log')).write_text(p.stdout+p.stderr)
            assert p.returncode==0 and ('c VERIFIED' if mode else 's VERIFIED') in p.stdout
            records.append(dict(name=name,command=command,checker_sha256=sha(checker),input_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size))
            proof.unlink(); cnf.unlink()
(c/'summary.json').write_text(json.dumps(dict(records=records,checked=len(records),fixture_sha256=sha(fixture),binary_sha256=sha(c/'fixture'),
    cleanup='All twelve generated fixed CNFs and checked proofs deleted; fixture binaries/logs and hashes retained.'),indent=2)+'\n')
print('PASS:',len(records),'independent fixed DRAT/LRAT certificates; ten-model and late-fixed fixture per case')
