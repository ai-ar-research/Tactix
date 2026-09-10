#!/usr/bin/env python3
"""Recheck the current collector's fixed root/reason DRAT and LRAT oracles."""
from pathlib import Path
import json
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6734-reduce-watermark-proof-control'
(c/'logs').mkdir(parents=True,exist_ok=False)
proofs=c/'fixed-proofs'; proofs.mkdir()
binary=root/'build/pin-6730-reduce-watermark-release/tests/test_reduce_root_lock'
def run(name,command):
    r=subprocess.run(list(map(str,command)),cwd=c,capture_output=True,text=True)
    with (c/'logs'/(name+'.log')).open('x') as out: out.write(r.stdout+r.stderr)
    assert r.returncode==0,(name,r.returncode)
    return r.stdout+r.stderr
run('root-lock',[binary,proofs])
records=[]
for cnf in sorted(proofs.glob('*.cnf')):
    proof=cnf.with_suffix('.proof'); lrat=cnf.stem.endswith('-lrat')
    checker=root/'tools/checkers/bin'/('lrat-check' if lrat else 'drat-trim')
    output=run('check-'+cnf.stem,[checker,cnf,proof])
    assert ('c VERIFIED' if lrat else 's VERIFIED') in output
    records.append(dict(input=str(cnf.relative_to(root)),input_sha256=sha(cnf),
        certificate_sha256=sha(proof),checker_sha256=sha(checker)))
assert len(records)==8
paths=[binary,Path(__file__),root/'benchmarks/runs/20260909-6730-reduce-watermark-build/manifest.json',
       root/'benchmarks/runs/20260909-6733-reduce-watermark-native/summary.json',*(c/'logs').iterdir(),*proofs.iterdir()]
with (c/'summary.json').open('x') as out:
    json.dump(dict(checked_proofs=8,records=records,hashes={str(p.relative_to(root)):sha(p) for p in paths},
        scope='Inherited fixed collector controls on current6730:16 root/elevated compact/tail SAT/UNSAT DRAT/LRAT cases, direct model checks and8 independent proof checks, plus protected nonroot/pending reasons. New288 watermark fixtures and native3 cases are structural controls, not proof certificates. No fuzz or performance claim.'),out,indent=2)
    out.write('\n')
print('8 independent collector proofs PASS')
