"""Fixed Python/session integration with independent certificate oracles."""
from pathlib import Path
import hashlib
import gc
import importlib.util
import json
import subprocess
import struct
import sys
import tempfile
from test_native_query_proof import formula
from test_native_query_append_proof import satisfiable
library,root=map(Path,sys.argv[1:])
sys.path.insert(0,str(root/'python'))
sys.path.insert(0,str(root/'tools/bench'))
from tactix import Session, ScriptedCDCL, SAT, UNSAT, UNKNOWN
from python_policy_reference import Cruxmiter, ScriptedDefault
spec=importlib.util.spec_from_file_location('python_session_runner',root/'tools/bench/run.py')
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
from proof_oracle import certificate_route
for tool in (runner.VERIPB,runner.CAKE_PB_CNF,runner.DRAT_TRIM,runner.CAKE_LPR,runner.DSR_TRIM,runner.DSR_COMPRESS,runner.SR_CHECK):
    if not Path(tool).is_file():sys.exit(77)
def verify(stem):
    cnf,pf=Path(str(stem)+'.cnf'),Path(str(stem)+'.proof')
    n,clauses=formula(cnf)
    meta_path=Path(str(stem)+'.json')
    if meta_path.exists():
        meta=json.loads(meta_path.read_text())
        for suffix,digest in meta['sha256'].items():
            assert hashlib.sha256(Path(str(stem)+suffix).read_bytes()).hexdigest()==digest
        if meta['result']==SAT:
            data=Path(str(stem)+'.model').read_text().split()
            assert data[0]=='v' and data[-1]=='0'
            values={abs(int(x)):int(x)>0 for x in data[1:-1]}
            assert len(values)==n and all(any(values[abs(x)]==(x>0) for x in c) for c in clauses)
            return
    assert not satisfiable(n,clauses)
    kind='pbp' if pf.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
    route='pbp' if kind=='pbp' else 'dsr-cake_lpr' if certificate_route(pf)=='drat' else 'dsr-trestle'
    checked=runner.check_proof(kind,str(cnf),str(pf),30,route=route)
    assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),(stem,checked)

with tempfile.TemporaryDirectory(prefix='python-tactix-') as tmp:
    tmp=Path(tmp)
    for name,strategy in (('default',None),('scripted',ScriptedCDCL(lambda c:-c[0])),
                          ('cruxmiter','cruxmiter'),('chvatal','chvatal'),('dpll','dpll'),('cdcl-no-restarts','cdcl-no-restarts'),
                          ('python-cruxmiter',Cruxmiter()),
                          ('scripted-default','scripted-default'),
                          ('python-scripted-default',ScriptedDefault())):
        folder=tmp/name;folder.mkdir()
        with Session(library=library,strategy=strategy,core_budget=1_000_000) as s:
            if isinstance(strategy,Cruxmiter):strategy.configure(s)
            s.add([1]);source=s.solve([7,-1]);assert source.status==UNSAT and source.core==(-1,)
            current=s.solve();assert current.status==SAT
            expected=hashlib.sha256(b'tactix-permanent-input-i32le-v1\n'+struct.pack('<ii',1,0)).hexdigest()
            assert source.input_sha256==current.input_sha256==expected
            before=s._lib.dll.tactix_ipasir_bundle(s._handle)
            reduced=s.minimize_core(source);assert reduced.minimal and reduced.core==(-1,) and reduced.source is source
            assert s._lib.dll.tactix_ipasir_bundle(s._handle)==before and s._lib.dll.ipasir_val(s._handle,1)==1
            assert not s._lib.dll.ipasir_failed(s._handle,-1)
            assert len(reduced.witnesses)==1 and reduced.witnesses[0][0]==-1
            assert reduced.certificate.status==UNSAT
            source.export(folder/'source');reduced.certificate.export(folder/'minimal')
            for i,probe in enumerate(reduced.probes):probe.export(folder/f'probe-{i}')
            limited=s.minimize_core(source,max_probes=0);assert not limited.minimal and limited.core==(7,-1)
            for invalid in (-1,2**64):
                try:s.minimize_core(source,max_probes=invalid)
                except ValueError:pass
                else:raise AssertionError('invalid native probe budget accepted')
            contextual=s.minimize_core(source,context=(-1,));assert contextual.minimal and contextual.core==() and contextual.context==(-1,)
            contextual.certificate.export(folder/'context')
            s.set_terminate(lambda:True)
            interrupted=s.minimize_core(source);assert not interrupted.minimal and interrupted.probes[0].status==UNKNOWN
            assert interrupted.certificate is source and source.core==(-1,)
            s.set_terminate(None);assert s.solve().status==SAT
            s.add([2])
            try:s.minimize_core(source)
            except ValueError:pass
            else:raise AssertionError('stale prefix accepted')
        assert Path(source.bundle+'.json').is_file()
        source.export(folder/'after-close');verify(folder/'after-close')
        assert source.input_sha256==expected
        for probe in reduced.probes:
            assert Path(probe.bundle+'.json').is_file()
        for stem in folder.glob('*.json'):verify(stem.with_suffix(''))
        verify(folder/'source-core')
        with Session(library=library) as other:
            try:other.minimize_core(source)
            except ValueError:pass
            else:raise AssertionError('foreign source accepted')
        if isinstance(strategy,ScriptedCDCL):assert strategy.calls>5 and strategy.steps>strategy.calls
    with Session(library=library,strategy='cruxmiter') as s:
        s.add([1,2]);assert s.solve().status==SAT
        assert s.strategy_name=='cruxmiter' and s.strategy_counters()['query_entries']==1
        assert s.strategy_counters()['root_requests']>0
        try:s.set_strategy('missing')
        except ValueError:pass
        else:raise AssertionError('unknown native strategy accepted')
        assert s.strategy_name=='cruxmiter'
        s.set_strategy(None);assert s.strategy_name=='default' and s.solve().status==SAT
    # A result keeps native files alive even after the Session object is gone;
    # dropping the last result releases the shared artifact directory.
    def detached():
        with Session(library=library) as session:
            session.add([1]);return session.solve()
    detached_result=detached();directory=Path(detached_result.bundle).parent
    gc.collect();assert directory.is_dir()
    detached_result.export(tmp/'detached');verify(tmp/'detached')
    del detached_result;gc.collect();assert not directory.exists()
    stop=[False]
    def interrupted_assumption(execution,restored):
        assert execution.next()==1
        stop[0]=True
        execution.assume_next()
        raise AssertionError('terminated assumption returned normally')
    with Session(library=library,strategy=interrupted_assumption) as s:
        s.set_terminate(lambda:stop[0])
        assert s.solve([5]).status==UNKNOWN
        s.set_terminate(None);s.set_strategy(ScriptedCDCL())
        resumed=s.solve();assert resumed.status==SAT and not resumed.assumptions
    # Exceptions stay in Python, UNKNOWN consumes A, and borrowed views expire.
    def forced_policy():
        return ScriptedCDCL(lambda candidates:-1 if any(abs(x)==1 for x in candidates) else candidates[0])
    learned=[]
    with Session(library=library,strategy=forced_policy()) as s:
        s.add([1,2]);s.add([1,-2]);s.set_learn(learned.append,1)
        answer=s.solve();assert answer.status==SAT and learned==[(1,)]
        answer.export(tmp/'learned-source');verify(tmp/'learned-source')
        assert s.solve([-1]).status==UNSAT
        assert all(clause==(1,) for clause in learned)
        s.set_learn(None,0);count=len(learned);assert s.solve().status==SAT and len(learned)==count
    with Session(library=library,strategy=forced_policy()) as s:
        s.add([1,2]);s.add([1,-2]);learned=[];s.set_learn(learned.append,1,work_budget=0)
        assert s.solve().status==SAT and not learned
    views=[]
    forced=forced_policy()
    def capture(execution,restored):
        views.append(execution)
        return forced(execution,restored)
    with Session(library=library,strategy=capture) as s:
        s.add([1,2]);s.add([1,-2]);s.set_learn(lambda clause:views[-1].next(),1)
        try:s.solve()
        except RuntimeError as e:assert isinstance(e.__cause__,RuntimeError) and 'callback' in str(e.__cause__)
        else:raise AssertionError('view callback re-entry accepted')
        s.set_learn(None,0);s.set_strategy(None);assert s.solve().status==SAT
    saved=[]
    def faulty(execution,restored):
        saved.append(execution)
        raise ValueError('intentional callback failure')
    with Session(library=library,strategy=faulty) as s:
        try:s.solve([3])
        except RuntimeError as e:assert isinstance(e.__cause__,ValueError)
        else:raise AssertionError('callback exception lost')
        try:saved[0].next()
        except RuntimeError:pass
        else:raise AssertionError('expired execution view accepted')
        s.set_strategy(None);assert s.solve().status==SAT
    with Session(library=library) as s:
        s.add([1]);source=s.solve([7,-1]);current=s.solve()
        before=s._lib.dll.tactix_ipasir_bundle(s._handle)
        s.set_strategy(faulty)
        try:s.minimize_core(source)
        except RuntimeError as e:assert isinstance(e.__cause__,ValueError)
        else:raise AssertionError('native minimization callback exception lost')
        assert s._lib.dll.tactix_ipasir_bundle(s._handle)==before and s._lib.dll.ipasir_val(s._handle,1)==1
        s.set_strategy(None);assert s.solve().status==SAT
print('Python sessions: default/scripted/native-and-Python-cruxmiter/native-and-Python-scripted-default/Chvatal/DPLL/no-restart cores, minimization, learned exports, contexts, interruption and lifetimes pass')
