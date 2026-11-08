"""Native named strategy: exact original queries, models and reduced-core proofs."""
from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import sys
import tempfile
from test_native_query_proof import formula
from test_native_query_append_proof import satisfiable

fixture,root=map(Path,sys.argv[1:3])
name=sys.argv[3] if len(sys.argv)>3 else 'cruxmiter'
sys.path.insert(0,str(root/'tools/bench'))
spec=importlib.util.spec_from_file_location('crux_runner',root/'tools/bench/run.py')
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
from proof_oracle import certificate_route
for tool in (runner.VERIPB,runner.CAKE_PB_CNF,runner.DRAT_TRIM,runner.CAKE_LPR,
             runner.DSR_TRIM,runner.DSR_COMPRESS,runner.SR_CHECK):
    if not Path(tool).is_file():sys.exit(77)
base=[[-3,1],[-3,2],[3,-1,-2],[-4,1],[-4,2],[4,-1,-2]]
proofs=models=reduced=0
with tempfile.TemporaryDirectory(prefix='ipasir-cruxmiter-proof-') as tmp:
    prefix=Path(tmp)/'q'
    run=subprocess.run([fixture,prefix,name],capture_output=True,text=True,timeout=60)
    assert not run.returncode,(run.stdout,run.stderr)
    for i,(status,assumptions) in enumerate(((20,[-3,4]),(10,[3]),(10,[-4]),(20,[7,-4]),(10,[])),1):
        stem=Path(str(prefix)+f'-{i}');meta=json.loads(Path(str(stem)+'.json').read_text())
        assert meta['result']==status
        for suffix,digest in meta['sha256'].items():
            assert hashlib.sha256(Path(str(stem)+suffix).read_bytes()).hexdigest()==digest
        n,clauses=formula(Path(str(stem)+'.cnf'));permanent=base+([[3]] if i>=4 else [])
        assert clauses==permanent+[[a] for a in assumptions]
        assert satisfiable(n,clauses)==(status==10)
        if status==10:
            words=Path(str(stem)+'.model').read_text().split();assert words[0]=='v' and words[-1]=='0'
            values={abs(int(x)):int(x)>0 for x in words[1:-1]}
            assert len(values)==n and all(any(values[abs(x)]==(x>0) for x in c) for c in clauses)
            models+=1;continue
        core=list(map(int,Path(str(stem)+'.core').read_text().split()));assert core.pop()==0
        assert set(core)<=set(assumptions) and not satisfiable(n,permanent+[[a] for a in core])
        if i==4:assert core==[-4] and meta['core']=='licensed-subset'
        stems=[stem]+([Path(str(stem)+'-core')] if meta['core']=='licensed-subset' else [])
        for selected in stems:
            cnf,pf=Path(str(selected)+'.cnf'),Path(str(selected)+'.proof')
            if selected!=stem:
                assert formula(cnf)[1]==permanent+[[a] for a in core];reduced+=1
            kind='pbp' if pf.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
            route='pbp' if kind=='pbp' else 'dsr-cake_lpr' if certificate_route(pf)=='drat' else 'dsr-trestle'
            checked=runner.check_proof(kind,str(cnf),str(pf),30,route=route)
            assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),(selected,checked)
            proofs+=1
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
                pf=Path(str(stem)+'.proof');kind='pbp' if pf.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
                route='pbp' if kind=='pbp' else 'dsr-cake_lpr' if certificate_route(pf)=='drat' else 'dsr-trestle'
                checked=runner.check_proof(kind,str(stem)+'.cnf',str(pf),30,route=route)
                assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),(scenario,mode,checked);proofs+=1
    print(f'{name}: {models} original-query models and {proofs} proofs ({reduced} reduced-core); plain no-proof/DSR/PBP/auto counters agree')
