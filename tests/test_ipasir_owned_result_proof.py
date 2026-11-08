"""Check files copied from owned C handles after the originating session closes."""
from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import sys
import tempfile
from test_native_query_proof import formula
from test_native_query_append_proof import satisfiable

fixture, root = map(Path, sys.argv[1:])
sys.path.insert(0, str(root/'tools/bench'))
spec = importlib.util.spec_from_file_location('owned_runner', root/'tools/bench/run.py')
runner = importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
from proof_oracle import certificate_route
for tool in (runner.VERIPB,runner.CAKE_PB_CNF,runner.DRAT_TRIM,runner.CAKE_LPR,
             runner.DSR_TRIM,runner.DSR_COMPRESS,runner.SR_CHECK):
    if not Path(tool).is_file():sys.exit(77)

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
    kind = 'pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
    route = 'pbp' if kind=='pbp' else 'dsr-cake_lpr' if certificate_route(proof)=='drat' else 'dsr-trestle'
    checked = runner.check_proof(kind,str(cnf),str(proof),30,route=route)
    assert checked[:3] == (True,'VERIFIED',runner.CHK_VERIFIED),(stem,checked)

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
