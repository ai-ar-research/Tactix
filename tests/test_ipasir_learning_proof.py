"""Public learned exports must follow from F without assumptions or SR roots."""
from proof_test_support import check_unsat, formula, proof_runner, satisfiable
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
fixture,root=map(Path,sys.argv[1:])
runner = proof_runner(root)

with tempfile.TemporaryDirectory(prefix='ipasir-learning-proof-') as tmp:
    prefix=Path(tmp)/'q';run=subprocess.run([fixture,prefix],capture_output=True,text=True,timeout=45)
    assert run.returncode==0,(run.stdout,run.stderr)
    for i,base in enumerate(([[1,2],[1,-2]],[[1,2],[1,-2]],[],[[1],[-1]]),1):
        stem=Path(str(prefix)+f'-{i}');meta=json.loads(Path(str(stem)+'.json').read_text());n,clauses=formula(Path(str(stem)+'.cnf'))
        for suffix,digest in meta['sha256'].items():assert hashlib.sha256(Path(str(stem)+suffix).read_bytes()).hexdigest()==digest
        learned=[]
        for line in Path(str(stem)+'.learned').read_text().splitlines():
            body=list(map(int,line.split()));assert body[-1]==0;body=body[:-1];learned.append(body)
            assert not satisfiable(n,base+[[-l] for l in body]),(i,base,body)
        assert learned==([[1]] if i<=2 else [] if i==3 else [[]]),(i,learned)
        if meta['result']==10:
            data=Path(str(stem)+'.model').read_text().split();values={abs(int(x)):int(x)>0 for x in data[1:-1]}
            assert data[0]=='v' and data[-1]=='0' and all(any(values[abs(x)]==(x>0) for x in c) for c in clauses)
            continue
        proof=Path(str(stem)+'.proof');check_unsat(runner, str(stem) + '.cnf', str(proof))
print('Public learning: four original-query bundles, independent F-only export oracles, three checked UNSAT proofs and one checked SAT model')
