from proof_test_support import check_unsat, formula, proof_runner, satisfiable
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
fixture,root=map(Path,sys.argv[1:])
runner = proof_runner(root)

def verify(stem):
    cnf,proof=Path(str(stem)+'.cnf'),Path(str(stem)+'.proof')
    n,cs=formula(cnf);assert not satisfiable(n,cs)
    return check_unsat(runner, str(cnf), str(proof))
with tempfile.TemporaryDirectory(prefix='ipasir-core-proof-') as tmp:
    prefix=Path(tmp)/'q'
    run=subprocess.run([fixture,prefix],capture_output=True,text=True,timeout=45)
    assert run.returncode==0,(run.stdout,run.stderr)
    for i,core in enumerate(([-1],[7,-1],[9,-9],[]),1):
        stem=Path(str(prefix)+f'-{i}');meta=json.loads(Path(str(stem)+'.json').read_text())
        actual=list(map(int,Path(str(stem)+'.core').read_text().split()));assert actual==core+[0]
        for suffix,digest in meta['sha256'].items():assert hashlib.sha256(Path(str(stem)+suffix).read_bytes()).hexdigest()==digest
        verify(stem)
        if i==2:
            assert meta['core']=='full-original-cube';continue
        assert meta['core']=='licensed-subset'
        reduced=Path(str(stem)+'-core');kind,route=verify(reduced)
        _,clauses=formula(Path(str(reduced)+'.cnf'))
        base={1:[[1]],3:[],4:[[1],[-1]]}[i]
        assert clauses==base+[[a] for a in core],(i,clauses)
        proof=Path(str(reduced)+'.proof')
        wrong=Path(tmp)/f'sat-control-{i}.cnf'
        wrong.write_text(f'p cnf 9 {len(clauses)}\n'+''.join('1 -1 0\n' for _ in clauses))
        rejected=runner.check_proof(kind,str(wrong),str(proof),30,route=route)
        assert not rejected[0] and rejected[2]==runner.CHK_REJECTED,(i,rejected)
    print('IPASIR core bundles: four query proofs, three reduced-core proofs and three SAT-source rejects independently verified')
