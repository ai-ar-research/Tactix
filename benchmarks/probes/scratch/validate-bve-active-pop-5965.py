#!/usr/bin/env python3
"""Actual old/new/native controls and eight independently checked refutations."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5962-bve-active-pop'
out=c/'controls';out.mkdir(exist_ok=False)
new=root/'build/pin-5962-bve-active-pop-release/tests/test_bve_active_pop'
fixture=root/'.claude/worktrees/runtime-bve-active-pop-5962/tests/test_bve_active_pop.cpp'
oldbuild=root/'build/pin-5900-reduce-root-release'
oldsource=root/'.claude/worktrees/runtime-reduce-root-5900'
native=root/'tools/solvers/variants/kissat-imply-fast-5414'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(name,cmd):
    r=subprocess.run(cmd,cwd=root,capture_output=True,text=True)
    (out/(name+'.log')).write_text(r.stdout+r.stderr)
    assert r.returncode==0,(name,r.returncode,r.stdout,r.stderr)
    return r.stdout
objects=sorted((oldbuild/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
run('old-build',['g++','-std=c++20','-O2','-fno-exceptions','-fno-rtti','-DTACTIX_EXPECT_OLD_BVE_GHOST',
    '-I'+str(oldsource/'src'),'-I'+str(oldsource/'include'),str(fixture),*map(str,objects),'-o',str(out/'old')])
run('native-build',['gcc','-DNDEBUG','-O2','-I'+str(native/'src'),str(Path(__file__).with_name('bve-active-pop-native-5965.c')),
    str(native/'build/libkissat.a'),'-lm','-o',str(out/'native')])
outputs={name:run(name,[exe]) for name,exe in [('old',out/'old'),('new',new),('native',out/'native')]}
assert len(outputs['old'].splitlines())==len(outputs['new'].splitlines())==24
assert len(outputs['native'].splitlines())==6
proofs=c/'fixed-proofs';proofs.mkdir(exist_ok=False)
run('proof-fixtures',[new,proofs/'bve-active'])
records=[]
for cnf in sorted(proofs.glob('*.cnf')):
    proof=cnf.with_suffix('.proof');lrat=cnf.stem.endswith('-lrat')
    checker=root/'tools/checkers/bin'/('lrat-check' if lrat else 'drat-trim')
    cmd=[str(checker),str(cnf),str(proof)]
    text=run('check-'+cnf.stem,cmd)
    assert ('c VERIFIED' if lrat else 's VERIFIED') in text
    records.append(dict(cnf=str(cnf.relative_to(root)),proof_sha256=sha(proof),cnf_sha256=sha(cnf),checker_sha256=sha(checker),command=cmd))
assert len(records)==8
(out/'summary.json').write_text(json.dumps(dict(old_new_cases=24,native_cases=6,checked_proofs=8,records=records,
    hashes={str(p.relative_to(root)):sha(p) for p in [fixture,new,out/'old',out/'native',*(out.glob('*.log'))]},
    outcome='Old policy counts4 for3 retired variables on two-sided large cases;new andnative count3. Public retry-independent instance offering,SAT models,invariants andeight DRAT/LRAT refutations pass. No fuzz or runtime claim.'),indent=2)+'\n')
print('PASS:24 actual old/new cases,6 native cases,8 independently checked proofs')
