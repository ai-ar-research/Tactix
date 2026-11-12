"""Fixed Python/session integration with independent certificate oracles."""
from proof_test_support import check_unsat, formula, proof_runner, satisfiable
from pathlib import Path
import hashlib
import gc
import json
import subprocess
import struct
import sys
import tempfile
library,root=map(Path,sys.argv[1:])
sys.path.insert(0,str(root/'python'))
from tactix import Session, ScriptedCDCL, SAT, UNSAT, UNKNOWN, capi
from tactix.strategies import Cruxmiter
runner = proof_runner(root)

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
    return check_unsat(runner, str(cnf), str(pf))

with tempfile.TemporaryDirectory(prefix='python-tactix-') as tmp:
    tmp=Path(tmp)
    for driver in ('cruxmiter','chvatal','dpll','cdcl-no-restarts','scripted-default'):
        with Session(library=library,strategy=driver) as solver:
            assert solver.strategy_name == driver
    for name,strategy in (('default',None),('scripted',ScriptedCDCL(lambda c:-c[0])),
                          ('cruxmiter','cruxmiter'),('python-cruxmiter',Cruxmiter())):
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
    # Files live through the session; afterwards each result pins only its query.
    with Session(library=library) as retained:
        retained.add([1]);first=retained.solve();directory=Path(first.bundle).parent
        for index in range(16):
            latest=retained.solve();assert latest.status==SAT
            assert len(list(directory.glob('*.json')))==index+2
            del latest
        verify(first.bundle)
    assert set(directory.glob('*.json'))=={Path(first.bundle+'.json')}
    members=json.loads(Path(first.bundle+'.json').read_text())['sha256']
    assert set(directory.iterdir())=={Path(first.bundle+suffix) for suffix in (*members,'.json')}
    del first;gc.collect();assert not directory.exists()
    # A failed writer must leave the still-live old epoch's files intact.
    with Session(library=library) as retry:
        retry.add([1]);old=retry.solve();prefix=old.bundle;directory=Path(prefix).parent
        del old
        retry.set_strategy(lambda execution,restored:0)
        assert retry.solve().status==UNKNOWN
        unpublished=directory/'q2-a2.cnf';assert unpublished.is_file()
        (directory/'q3-a3.cnf').mkdir() # fopen must fail, including as root
        assert retry.solve().status==UNKNOWN
        assert unpublished.is_file()
        retry.set_strategy(None)
        recovered=retry.solve();assert recovered.status==SAT
        assert not unpublished.exists() and Path(prefix+'.json').is_file()
        verify(recovered.bundle)
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
    # Lucky's all-true candidate satisfies F but misses A. This observation
    # lets a Python root scheduler make the same explicit rejection restart
    # as native DefaultPolicy, without treating progress as query SAT.
    rejected=[]
    def candidate_policy(execution,restored):
        assert execution.rejected_models==0
        assert execution.apply('Lucky',0)==0
        count=execution.rejected_models
        assert count==1
        for _ in range(32):assert execution.c.tactix_value(1)==0
        assert execution.rejected_models==count
        assert execution.apply('Restart',0)>=0
        assert execution.rejected_models==count
        rejected.append(execution)
        return ScriptedCDCL()(execution,restored)
    with Session(library=library,strategy=candidate_policy) as s:
        s.add([1,2])
        for i in range(2):
            result=s.solve([-1]);assert result.status==SAT and -1 in result.model
            result.export(Path(tmp)/f'rejected-model-{i}');verify(Path(tmp)/f'rejected-model-{i}')
        assert s._lib.dll.tactix_ipasir_rejected_models(s._handle)==2**64-1
    assert s._lib.dll.tactix_ipasir_rejected_models(None)==2**64-1
    try:rejected[0].rejected_models
    except RuntimeError:pass
    else:raise AssertionError('expired model-rejection observation accepted')
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
    # Callback conversion happens outside the Python callback's exception
    # handler in ctypes. Reject invalid results before crossing that boundary;
    # failures still consume assumptions and expire borrowed execution views.
    for invalid in (None, '10', 10.0, 10 + 2**32, 1, object()):
        views=[]
        def invalid_result(execution,restored):
            views.append(execution)
            return invalid
        with Session(library=library,strategy=invalid_result) as s:
            try:s.solve([3])
            except RuntimeError as e:
                assert isinstance(e.__cause__,ValueError) and 'strategy must return' in str(e.__cause__)
            else:raise AssertionError('invalid strategy return accepted')
            try:views[-1].next()
            except RuntimeError:pass
            else:raise AssertionError('invalid-return execution view remained active')
            s.set_strategy(None)
            result=s.solve()
            assert result.status==SAT and result.assumptions==()
    with Session(library=library) as s:
        s.add([1]);source=s.solve([7,-1]);current=s.solve()
        before=s._lib.dll.tactix_ipasir_bundle(s._handle)
        for strategy in (faulty, lambda execution,restored:None):
            s.set_strategy(strategy)
            try:s.minimize_core(source)
            except RuntimeError as e:assert isinstance(e.__cause__,ValueError)
            else:raise AssertionError('native minimization callback exception lost')
            assert s._lib.dll.tactix_ipasir_bundle(s._handle)==before and s._lib.dll.ipasir_val(s._handle,1)==1
        s.set_strategy(None);assert s.solve().status==SAT
    # An exception must unwind contexts and discard an unsettled epoch, while
    # preserving the session's callbacks and permanent input for the next call.
    for phase in (False,True):
        views=[];polls=[]
        def abandon(execution,restored):
            views.append(execution)
            assert execution.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_CONTEXT,capi.TACTIX_CONTEXT_STARTUP)==0
            if phase:
                assert execution.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,0)==0
            assert execution.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_CONTEXT,capi.TACTIX_CONTEXT_SPECULATION)==0
            raise ValueError('abandoned execution')
        with Session(library=library,strategy=abandon) as s:
            s.add([1,2]);s.set_terminate(lambda:polls.append(1) or False)
            try:s.solve()
            except RuntimeError as e:assert isinstance(e.__cause__,ValueError)
            else:raise AssertionError('abandoned context exception lost')
            try:views[-1].next()
            except RuntimeError:pass
            else:raise AssertionError('abandoned view remained active')
            s.set_strategy(None);before=len(polls);result=s.solve([-1])
            assert result.status==SAT and result.model[1]==2 and len(polls)>before
            prefix=tmp/('abandoned-phase' if phase else 'abandoned-context')
            result.export(prefix);verify(prefix)
    calls=[];failure=ValueError('abort before restoration')
    def fail_before_restore(execution,restored):
        calls.append(restored)
        if len(calls)>1:return UNKNOWN
        assert execution.c.tactix_apply_instance(execution._lib.rules['BVE'],1)>=0
        assert execution.next()==capi.TACTIX_QUERY_RESTORE
        raise failure
    with Session(library=library,strategy=fail_before_restore) as s:
        s.add([1,2])
        try:s.solve([-1])
        except RuntimeError as e:assert e.__cause__ is failure
        else:raise AssertionError('restoration exception lost')
        assert calls==[False]
        s.set_strategy(None);assert s.solve([-1]).status==SAT
    def checked_add(execution,restored):
        try:execution.c.tactix_add
        except AttributeError:pass
        else:raise AssertionError('aborting add exposed on borrowed view')
        assert execution.c.tactix_add_checked(1)==-1
        return execution.default_solve()
    with Session(library=library,strategy=checked_add) as s:
        s.add([1]);assert s.solve().status==SAT
print('Python sessions: strategies, cores, proofs/models, callbacks, failure recovery and lifetimes pass')

# A full gathered BVE witness may retain a literal made false by its resolvent.
from contextlib import nullcontext
from tactix import BveOrder
import ctypes as C
handoff_models = handoff_proofs = 0
for ordered in (False, True):
    applied = []
    def bve_handoff(execution, restored):
        view = execution.c
        if view.tactix_status() == UNSAT: return UNSAT
        assert view.tactix_support(capi.TACTIX_SUPPORT_ROOT_PREPARATION, 0) == 0
        assert view.tactix_support(capi.TACTIX_SUPPORT_ENTER_INPROCESS_PHASE, capi.TACTIX_PHASE_ELIMINATE) == 0
        assert view.tactix_support(capi.TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0) == 0
        with BveOrder(execution, [3,2]) if ordered else nullcontext() as order:
            for variable in (3,2):
                instance = capi.TACTIX_LIT_INSTANCE(variable)
                state = capi.tactix_bve_candidate_state()
                assert view.tactix_read_bve_candidate(instance, C.byref(state)) == 0
                if state.flags & (capi.TACTIX_BVE_CANDIDATE_ASSIGNED | capi.TACTIX_BVE_CANDIDATE_RETIRED | capi.TACTIX_BVE_CANDIDATE_QUERY): continue
                before = view.tactix_stat(capi.TACTIX_STAT_BVE_ELIMINATED)
                result = order.apply('BVE', instance) if ordered else execution.apply('BVE', instance)
                assert result == 0 and view.tactix_stat(capi.TACTIX_STAT_BVE_ELIMINATED) == before+1
                applied.append(variable)
                if result == UNSAT: break
                if ordered:
                    result = order.propagate()
                else:
                    choices = (C.c_int * view.tactix_rule_count())()
                    count = view.tactix_rules_applicable(choices, len(choices))
                    result = execution.apply('Propagate') if execution._lib.rules['Propagate'] in choices[:count] else 0
                if result == UNSAT: break
        left = view.tactix_support(capi.TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0)
        assert left in (0, UNSAT)
        return UNSAT if left == UNSAT else execution.default_solve()
    with Session(library, strategy=bve_handoff) as session:
        for clause in ([-3,-6],[3,6],[-6,3],[-7,2]): session.add(clause)
        results = [session.solve()];assert results[0].status == SAT
        results.append(session.solve([6]));assert results[-1].status == UNSAT
        session.add([-3]);results.append(session.solve());assert results[-1].status == UNSAT
    assert applied == [3,2], applied
    with tempfile.TemporaryDirectory(prefix='bve-handoff-proof-') as raw:
        for index, result in enumerate(results):
            stem = Path(raw)/str(index);result.export(stem);certificate=verify(stem)
            if result.status == SAT: handoff_models += 1
            else:
                handoff_proofs += 1
                cnf,pf=Path(str(stem)+'.cnf'),Path(str(stem)+'.proof')
                n,original=formula(cnf)
                weakened=next(original[:i]+original[i+1:] for i in range(len(original))
                              if satisfiable(n,original[:i]+original[i+1:]))
                wrong=Path(raw)/(str(index)+'-weakened.cnf')
                wrong.write_text(f'p cnf {n} {len(weakened)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in weakened))
                kind,route=certificate
                checked=runner.check_proof(kind,str(wrong),str(pf),30,route=route)
                assert not checked[0] and checked[2]==runner.CHK_REJECTED, checked
assert handoff_models == 2 and handoff_proofs == 4
print(f'BVE to DefaultPolicy handoff: {handoff_models} models, {handoff_proofs} owned proofs/negative controls after Session release')

# Required callback setup must also execute when Python removes assertions.
subprocess.run([sys.executable, '-O', '-c', r'''
import sys
sys.path.insert(0, sys.argv[2]+'/python')
from tactix import Session, SAT, capi
from tactix.strategies import Cruxmiter
policy=Cruxmiter()
reached=[]
def run(execution, restored):
    result=policy(execution, restored)
    mode=execution.c.tactix_config(capi.TACTIX_CFG_GUIDANCE_PUBLICATION)
    if mode!=capi.TACTIX_GUIDANCE_NATIVE:
        raise RuntimeError('optimized Python skipped CRUX guidance setup')
    reached.append(policy.requests)
    return result
clauses=[[1,2],[-3,1],[-3,2],[3,-1,-2]]
with Session(sys.argv[1],strategy=run) as session:
    policy.configure(session)
    for clause in clauses:session.add(clause)
    answer=session.solve([-1])
    if answer.status!=SAT or not reached or max(reached)==0:
        raise RuntimeError('Python CRUX did not execute its schedule')
    values=set(answer.model)
    if -1 not in values or not all(any(l in values for l in c) for c in clauses):
        raise RuntimeError('invalid optimized CRUX model')
''', str(library), str(root)],check=True)
print('Optimized Python CRUX: guidance setup, reached schedule and model pass')
