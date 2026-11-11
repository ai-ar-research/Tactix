"""Proof-neutral learning, exact query proofs and independent F-only export checks."""
from proof_test_support import formula, proof_runner, satisfiable
from pathlib import Path
import subprocess
import sys
import tempfile
fixture,root=map(Path,sys.argv[1:])
runner = proof_runner(root)
from proof_oracle import certificate_route

with tempfile.TemporaryDirectory(prefix='learning-proof-') as tmp:
    trajectories={};poll_counts={}
    for scenario,mode in ((s,m) for s in ('plain','sr') for m in ('none','dsr','auto','pbp')):
        proofs=[]
        for enabled in (0,1):
            stem=Path(tmp)/f'{scenario}-{mode}-{enabled}'
            run=subprocess.run([fixture,stem,mode,str(enabled),scenario],capture_output=True,text=True,timeout=45)
            assert run.returncode==0,(scenario,mode,enabled,run.stdout,run.stderr)
            stats=dict(word.split('=') for word in run.stdout.split())
            polls=stats.pop('polls')
            assert stats==trajectories.setdefault(scenario,stats),(scenario,mode,enabled,stats)
            assert polls==poll_counts.setdefault((scenario,enabled),polls),(scenario,mode,enabled,polls,poll_counts)
            n,clauses=formula(Path(str(stem)+'.cnf'));assert clauses==[[1,2],[1,-2],[-1]]
            learned=[list(map(int,line.split()))[:-1] for line in Path(str(stem)+'.learned').read_text().splitlines()]
            assert learned==([[1]] if enabled else [])
            for clause in learned:assert not satisfiable(n,clauses[:-1]+[[-l] for l in clause])
            if mode=='none':continue
            proof=Path(str(stem)+'.proof');kind='pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
            route='pbp' if kind=='pbp' else 'dsr-cake_lpr' if certificate_route(proof)=='drat' else 'dsr-trestle'
            checked=runner.check_proof(kind,str(stem)+'.cnf',str(proof),30,route=route)
            assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),(scenario,mode,enabled,checked)
            proofs.append(proof.read_bytes())
        if proofs:assert proofs[0]==proofs[1],(scenario,mode)
print('Learning: twelve independently checked query proofs, F-only exports after SR, identical proof/search and termination polling across four proof modes')
