"""Public Python frontend setup versus tactix_public_main; controller still separate."""
from pathlib import Path
import ctypes as C
import importlib.util
import json
import re
import subprocess
import sys
import tempfile
library, fixture, public_main, root = map(Path, sys.argv[1:])
sys.path[:0] = [str(root/'python'), str(root/'tests'), str(root/'tools/bench')]
from tactix import Solver, Session, ScriptedCDCL, SAT, UNSAT, capi
from proof_oracle import certificate_route
spec=importlib.util.spec_from_file_location('frontend_oracles',root/'tools/bench/run.py')
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
for tool in (runner.VERIPB,runner.CAKE_PB_CNF,runner.DSR_TRIM,runner.DSR_COMPRESS,runner.SR_CHECK,runner.CAKE_LPR):
    assert Path(tool).is_file(),tool
stats=('VARIABLES','OUTPUT_VARIABLES','COLD_STATE_DEFERRED','HEURISTICS_ATTACHED')
def snapshot(s):
    return [s.c.tactix_stat(getattr(capi,'TACTIX_STAT_'+name)) for name in stats]+[s.c.tactix_config(capi.TACTIX_CFG_BVE_GATES)]
with tempfile.TemporaryDirectory(prefix='public-default-frontend-') as raw:
    directory=Path(raw)
    source=directory/'setup.cnf';source.write_text('p cnf 7 3\n1 2 0\n-1 3 0\n-3 4 0\n')
    native=[json.loads(line) for line in subprocess.check_output([str(fixture),str(source)],text=True).splitlines()]
    assert native[0][0]==native[0][1] and native[1][0]==native[1][1]
    with Solver(library,default_frontend=True) as s:
        s.read_dimacs(source);assert snapshot(s)==native[0][0]
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_ATTACH_HEURISTICS,0)==-1
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,1)==-1
        assert snapshot(s)==native[0][0]
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_CONTEXT,capi.TACTIX_CONTEXT_STARTUP)==0
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,0)==0
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_ATTACH_HEURISTICS,0)==0
        assert snapshot(s)==native[1][0]
        data=(C.c_int*s.c.tactix_stat(capi.TACTIX_STAT_VARIABLES))()
        count=s.c.tactix_decide_candidates(data,len(data));assert list(data[:count])==native[2]
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_CONTEXT,0)==0
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_DETACH_HEURISTICS,0)==0
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_DETACH_HEURISTICS,0)==1
    try:s.c.tactix_status()
    except RuntimeError:pass
    else:raise AssertionError('closed frontend view remained usable')
    def raises(kind, action):
        try:action()
        except kind:return
        raise AssertionError(f'expected {kind.__name__}')
    malformed=directory/'malformed.cnf';malformed.write_text('p cnf 2 1\n1 x 0\n')
    for frontend in (False,True):
        with Solver(library,default_frontend=frontend) as s:
            raises(RuntimeError,lambda:s.model)
            assert not s.c.tactix_stat(capi.TACTIX_STAT_INPUT_CLOSED)
            for query_only in (s.next,s.assume_next,lambda:s.rejected_models):
                raises(RuntimeError,query_only)
            raises(OSError,lambda:s.read_dimacs(directory/'missing.cnf'))
            s.add([1,2]);s.add([-1,2])
            raises(RuntimeError,lambda:s.model)
            assert not s.c.tactix_stat(capi.TACTIX_STAT_INPUT_CLOSED)
            s.add([1,-1])
            assert s.default_solve()==SAT
            assert 2 in s.model and len(s.model)==2
            raises(RuntimeError,lambda:s.add([3]))
            raises(ValueError,lambda:s.read_dimacs(source))
            raises(ValueError,lambda:s.proof(directory/'late.proof'))
        s.close()
        with Solver(library,default_frontend=frontend) as broken:
            raises(ValueError,lambda:broken.read_dimacs(malformed))
        broken.close()
    cases=[(7,[[1,2],[-1,3],[-3,4]],SAT),(2,[[1,2],[1,-2],[-1,2],[-1,-2]],UNSAT),
           (4,[[1]],SAT),(1,[[1],[-1]],UNSAT),
           (7,[[1,3],[1,4],[2,3],[2,4]],SAT)]
    models=proofs=0
    for index,(n,clauses,answer) in enumerate(cases):
        cnf=directory/f'{index}.cnf'
        cnf.write_text(f'p cnf {n} {len(clauses)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in clauses))
        for mode,fmt in [('none',None),('auto',capi.TACTIX_PROOF_AUTO),('pbp',capi.TACTIX_PROOF_PBP)]:
            first,second=directory/f'cpp-{index}-{mode}.proof',directory/f'python-{index}-{mode}.proof'
            command=[str(public_main)]+(['--pbp'] if mode=='pbp' else [])+[str(cnf)]+([str(first)] if fmt is not None else [])
            native=subprocess.run(command,capture_output=True,text=True,timeout=60)
            assert native.returncode==answer,native.stdout+native.stderr
            census=re.search(r'c public-port requests (\d+) fallbacks (\d+) declined (\d+)',native.stdout)
            assert census and census.group(2)=='0' and census.group(3)=='0',native.stdout
            with Solver(library,default_frontend=True) as s:
                if fmt is not None:s.proof(second,fmt)
                s.read_dimacs(cnf)
                # Explicit native completion only validates frontend equivalence.
                # This is not the still-unfinished Python DefaultPolicy controller.
                assert s.default_solve()==answer
                for name in ('conflicts','decisions','propagations'):
                    expected=int(re.search(r'^c '+name+r' (\d+)$',native.stdout,re.M).group(1))
                    assert s.c.tactix_stat(getattr(capi,'TACTIX_STAT_'+name.upper()))==expected
                if answer==SAT:
                    values=set(s.model)
                    assert len(values)==n and all((v in values)!=(-v in values) for v in range(1,n+1))
                    assert all(any(l in values for l in clause) for clause in clauses)
                    cpp=[int(x) for line in native.stdout.splitlines() if line.startswith('v ') for x in line.split()[1:] if x!='0']
                    assert values==set(cpp);models+=1
            if fmt is not None:
                assert first.read_bytes()==second.read_bytes(),(index,mode)
                if answer==UNSAT:
                    kind='pbp' if second.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
                    route='pbp' if kind=='pbp' else 'dsr-cake_lpr' if certificate_route(second)=='drat' else 'dsr-trestle'
                    checked=runner.check_proof(kind,str(cnf),str(second),30,route=route)
                    assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),checked
                    proofs+=1
    assert models==9 and proofs==4
def borrowed(e, restored):
    assert e.c.tactix_stat(capi.TACTIX_STAT_HEURISTICS_ATTACHED)==1
    assert e.c.tactix_support(capi.TACTIX_SUPPORT_ATTACH_HEURISTICS,0)==1
    assert e.c.tactix_support(capi.TACTIX_SUPPORT_DETACH_HEURISTICS,0)==-1
    assert e.c.tactix_support(capi.TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,0)==1
    # Reseeding is an existing explicitly selected policy action, not attach.
    assert e.c.tactix_support(capi.TACTIX_SUPPORT_RESEED_GUIDANCE,0)==0
    return ScriptedCDCL()(e,restored)
with Session(library,strategy=borrowed) as session:
    session.add([1,2]);result=session.solve();assert result.status==SAT and any(v in result.model for v in (1,2))
print(f'Public-default frontend: legacy/C/Python setup and guidance agree; 15 CLI/native completions, {models} models, {proofs} proofs; full Python controller remains open')
