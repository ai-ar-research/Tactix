"""Named C strategies preserve trajectories across proof formats."""
from proof_test_support import check_unsat, formula, proof_runner
from pathlib import Path
import subprocess
import sys
import tempfile

fixture,root=map(Path,sys.argv[1:3])
name=sys.argv[3] if len(sys.argv)>3 else 'cruxmiter'
runner = proof_runner(root)

proofs=models=0
with tempfile.TemporaryDirectory(prefix='ipasir-cruxmiter-proof-') as tmp:
    traces={}
    for scenario in ('sat','unsat'):
        for mode in ('none','dsr','pbp','auto'):
            stem=Path(tmp)/f'plain-{scenario}-{mode}'
            done=subprocess.run([fixture,mode,stem,scenario,name],capture_output=True,text=True,timeout=60)
            assert done.returncode==0,(scenario,mode,done.stdout,done.stderr)
            assert done.stdout==traces.setdefault(scenario,done.stdout),(scenario,mode,done.stdout)
            n,clauses=formula(Path(str(stem)+'.cnf'))
            if scenario=='sat':
                model=list(map(int,Path(str(stem)+'.model').read_text().split()));assert model.pop()==0
                assert len(model)==n and all(any(l in model for l in c) for c in clauses);models+=1
            elif mode!='none':
                pf=Path(str(stem)+'.proof');check_unsat(runner, str(stem) + '.cnf', str(pf));proofs+=1
    print(f'{name}: {models} checked models and {proofs} proofs; no-proof/DSR/PBP/auto counters agree')
