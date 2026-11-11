"""Check files copied from owned C handles after the originating session closes."""
from proof_test_support import check_unsat, formula, proof_runner, satisfiable
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile

fixture, root = map(Path, sys.argv[1:])
runner = proof_runner(root)


def verify(stem, status):
    cnf, proof = Path(str(stem)+'.cnf'), Path(str(stem)+'.proof')
    n, clauses = formula(cnf)
    if status == 10:
        words = Path(str(stem)+'.model').read_text().split()
        assert words[0] == 'v' and words[-1] == '0'
        values = {abs(int(x)):int(x)>0 for x in words[1:-1]}
        assert len(values)==n and all(any(values[abs(x)]==(x>0) for x in c) for c in clauses)
        return
    assert not satisfiable(n, clauses)
    check_unsat(runner, str(cnf), str(proof))

with tempfile.TemporaryDirectory(prefix='ipasir-owned-proof-') as tmp:
    prefix = Path(tmp)/'q'
    run = subprocess.run([fixture,prefix],capture_output=True,text=True,timeout=45)
    assert run.returncode==0,(run.stdout,run.stderr)
    for i,status in ((1,20),(2,10),(3,20)):
        stem = Path(str(prefix)+f'-{i}')
        meta = json.loads(Path(str(stem)+'.json').read_text());assert meta['result']==status
        for suffix,digest in meta['sha256'].items():
            assert hashlib.sha256(Path(str(stem)+suffix).read_bytes()).hexdigest()==digest
        verify(stem,status)
        if meta['core']=='licensed-subset':verify(Path(str(stem)+'-core'),20)
    print('Owned-result artifacts: two query proofs, one reduced-core proof and one original-query model verified after session release')
