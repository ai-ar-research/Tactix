#!/usr/bin/env python3
"""Actual native caller controls plus eight independent proof checks."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-6049-reduce-root-lock'
out=c/'controls';out.mkdir(exist_ok=False)
proofs=c/'fixed-proofs';proofs.mkdir(exist_ok=False)
native=root/'tools/solvers/variants/kissat-imply-fast-5414'
fixture=Path(__file__).with_name('reduce-root-lock-native-6052.c')
new=root/'build/pin-6049-reduce-root-lock-release/tests/test_reduce_root_lock'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(name,cmd):
    r=subprocess.run(cmd,cwd=root,capture_output=True,text=True)
    (out/(name+'.log')).write_text(r.stdout+r.stderr)
    assert r.returncode==0,(name,r.returncode,r.stdout,r.stderr)
    print(name,'PASS',flush=True)
    return r.stdout+r.stderr
run('native-build',['gcc','-DNDEBUG','-O2','-I'+str(native/'src'),str(fixture),str(native/'build/libkissat.a'),'-lm','-o',str(out/'native')])
outputs={'native':run('native',[out/'native']), 'new':run('new',[new,str(proofs)])}
records=[]
for cnf in sorted(proofs.glob('*.cnf')):
    p=cnf.with_suffix('.proof');lrat=cnf.stem.endswith('-lrat')
    checker=root/'tools/checkers/bin'/('lrat-check' if lrat else 'drat-trim')
    command=[str(checker),str(cnf),str(p)]; text=run('check-'+cnf.stem,command)
    assert ('c VERIFIED' if lrat else 's VERIFIED') in text
    records.append(dict(cnf=str(cnf.relative_to(root)),cnf_sha256=sha(cnf),proof_sha256=sha(p),checker_sha256=sha(checker),command=command))
assert len(records)==8
(out/'summary.json').write_text(json.dumps(dict(checked_proofs=8,records=records,outputs=outputs,
    hashes={str(p.relative_to(root)):sha(p) for p in [fixture,new,out/'native',*(out.glob('*.log'))]},
    scope='Sixteen current root reason cases cover root/elevated state, compact/tail collection, SAT/UNSAT and DRAT/LRAT. Two non-root/pending stale-lock controls. Native actual flush/collect confirms root admission and non-root protection. No fuzz or performance claim.'),indent=2)+'\n')
