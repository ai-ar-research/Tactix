"""Independent original-query and reduced-core oracles for dedicated C strategies."""
from proof_test_support import formula, proof_runner, satisfiable
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
fixture,root=map(Path,sys.argv[1:])
runner = proof_runner(root)
from proof_oracle import certificate_route

def proof(stem):
    cnf,pf=Path(str(stem)+'.cnf'),Path(str(stem)+'.proof')
    n,clauses=formula(cnf);assert not satisfiable(n,clauses)
    kind='pbp' if pf.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
    route='pbp' if kind=='pbp' else 'dsr-cake_lpr' if certificate_route(pf)=='drat' else 'dsr-trestle'
    checked=runner.check_proof(kind,str(cnf),str(pf),30,route=route)
    assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),(stem,checked)
with tempfile.TemporaryDirectory(prefix='ipasir-programmable-proof-') as tmp:
    prefix=Path(tmp)/'q'
    run=subprocess.run([fixture,prefix],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,(run.stdout,run.stderr)
    for i in range(1,14):
        stem=Path(str(prefix)+f'-{i}');meta=json.loads(Path(str(stem)+'.json').read_text())
        for suffix,digest in meta['sha256'].items():
            assert hashlib.sha256(Path(str(stem)+suffix).read_bytes()).hexdigest()==digest
        n,clauses=formula(Path(str(stem)+'.cnf'))
        if meta['result']==20:
            proof(stem)
            core=list(map(int,Path(str(stem)+'.core').read_text().split()))
            assert core[-1]==0
        else:
            assert meta['result']==10 and satisfiable(n,clauses)
            tokens=Path(str(stem)+'.model').read_text().split();assert tokens[0]=='v' and tokens[-1]=='0'
            literals=list(map(int,tokens[1:-1]));values={abs(x):x>0 for x in literals}
            assert len(values)==n and all(any(values[abs(x)]==(x>0) for x in c) for c in clauses)
    reduced=Path(str(prefix)+'-9-core');proof(reduced)
    assert formula(Path(str(reduced)+'.cnf'))[1]==[[1],[-1]]
    assert Path(str(prefix)+'-9.core').read_text().split()==['-1','0']
    preflight=Path(str(prefix)+'-11')
    assert formula(Path(str(preflight)+'.cnf'))[1]==[[-1,2],[-1,-2],[7],[1]]
    proof(Path(str(preflight)+'-core'))
    assert formula(Path(str(preflight)+'-core.cnf'))[1]==[[-1,2],[-1,-2],[1]]
    assert Path(str(preflight)+'.core').read_text().split()==['1','0']
    assert formula(Path(str(prefix)+'-13.cnf'))[1]==[[-1,2],[-1,-2],[1]]
    print('Dedicated strategies: thirteen exact queries and two strict reduced cores independently checked')
