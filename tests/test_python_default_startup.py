"""Python owns startup choices; compare the actual native policy's prefix."""
from pathlib import Path
import ctypes as C
import importlib.util
import subprocess
import sys
import tempfile
from contextlib import nullcontext
from unittest.mock import patch
library, fixture, root = map(Path, sys.argv[1:4])
full = sys.argv[4:]==['--full']
sys.path[:0] = [str(root/'python'), str(root/'tools/bench')]
from tactix import Solver, Session, ScriptedCDCL, capi
from tactix.startup import DefaultStartup, walk_preprocessing_deferred
from tactix.program import DefaultStage
from tactix.sequence import OnlineSequence
from proof_oracle import certificate_route
spec=importlib.util.spec_from_file_location('startup_oracles',root/'tools/bench/run.py')
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)

def pigeonhole(pigeons, holes):
    cell=lambda p,h:p*holes+h+1
    return [[cell(p,h) for h in range(holes)] for p in range(pigeons)]+[
        [-cell(p,h),-cell(q,h)] for p in range(pigeons) for q in range(p+1,pigeons) for h in range(holes)]

def graph_formula(n):
    cell=lambda row,column:row*n+column+1
    return [[cell(row,column) for row in range(n)] for column in range(n)]+[
        [-cell(row,a),-cell(row,b)] for row in range(n) for a in range(n) for b in range(a+1,n)]+[
        [-cell(a,0),-cell(b,0)] for a in range(n) for b in range(a+1,n)]

cases=[(3,[[1,2,3],[-1,-2,3],[-1,2,-3],[1,-2,-3],[1,-2],[-1,2]],{}),(16,graph_formula(4),{}),(3,[[1,2],[-1,3]],{}),(2,[[1,2],[1,-2],[-1,2],[-1,-2]],{}),
       (5,[[1]],{}),(1,[[1],[-1]],{}),(0,[],{}),
       (20,pigeonhole(5,4),{}),
       (3,[[a,b,c] for a in [-1,1] for b in [-2,2] for c in [-3,3]],{}),
       (3,[[1,2,3],[-1,-2,3],[-1,2,-3],[1,-2,-3]],{}),
       (3,[[1,2],[-1,3]],{'TACTIX_NO_PARITY_MODEL':'1','TACTIX_NO_ONEHOT_MODEL':'1'})]
deferred_case=helpers_case=None;cleanup_cases=set()
if full:
    cases.extend((16,graph_formula(4),options) for options in (
        {'TACTIX_TOUCH_MARKS':'0'}, {'TACTIX_TOUCH_REDUNDANT':'1'}))
    for options in ({},{'TACTIX_BCE_OCC_LIMIT':'1','TACTIX_TOUCH_MARKS':'0'},
                    {'TACTIX_TOUCH_REDUNDANT':'1'}):
        cleanup_cases.add(len(cases));cases.append((7,[[1,2,3],[4,6],[-4,7],[5,7]],options))
    helpers_case=len(cases);cases.append((7,[[1,2,3],[4,6],[-4,7],[5,7]],{}))
    import random
    generator=random.Random(7);triples=set()
    while len(triples)<550:triples.add(tuple(sorted(generator.sample(range(1,101),3))))
    paired=[clause for triple in sorted(triples) for clause in (list(triple),[-v for v in triple])]
    deferred_case=len(cases)
    cases.append((100,paired,{}))


class CleanupProgram:
    """Test program choice only: real C SR actions and real producer counters.

    Native uses the same custom stage in its actual DefaultPolicy call site.
    The substituted controller leaves the production placement/BCE tail intact.
    """
    family=capi.TACTIX_DEFAULT_STAGE_REGULAR
    query_rejected_model=False
    helpers_only=False
    def __init__(self,e,observer=None):
        self.e,self.observer=e,observer
        self.rule_ids={1:e._lib.rules['SubstitutionRedundancy'],2:e._lib.rules['SubstitutionRedundancy']}
    def run(self):
        with DefaultStage(self.e,self.family,1) as stage:
            source=stage.program
            selected=next(step for step in source.steps[:source.step_count]
                          if step.base.rule_id==self.rule_ids[1])
            steps=(capi.tactix_online_step_v2*2)(selected,selected)
            for index,step in enumerate(steps):
                step.base.step_id=index+1
                sr=step.base.parameters.value.substitution_redundancy
                sr.maximum_orbitopal_matrices=0
                sr.orbitopal_unique_literal_amo_mode=capi.TACTIX_ONLINE_SETTING_ENABLE
            steps[0].base.granularity=capi.TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS
            steps[1].base.granularity=capi.TACTIX_ONLINE_GRANULARITY_ONE
            steps[1].base.parameters.value.substitution_redundancy.producer=capi.TACTIX_ONLINE_SR_EXPLICIT_ONLY
            clause=(C.c_int*2)(5,6)
            cubes=(capi.tactix_online_sr_instance*1)(capi.tactix_online_sr_instance(clause,2,None,0,None,0))
            steps[1].sr_instances,steps[1].sr_instance_count=cubes,1
            program=capi.tactix_online_program_v2(steps,1 if self.helpers_only else 2,capi.TACTIX_ONLINE_REPEAT_ONCE,1)
            assert self.e.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_CONTEXT,capi.TACTIX_CONTEXT_INPROCESSING)>=0
            try:
                with OnlineSequence(self.e,program) as sequence:
                    for call in range(256):
                        if sequence.complete: break
                        dispatched=sequence.dispatches[:]
                        event=sequence.advance()
                        if self.observer and dispatched!=sequence.dispatches:
                            position=next(i for i in range(2) if dispatched[i]!=sequence.dispatches[i])
                            self.observer(0,position+1,event)
                        assert event.base.status in (capi.TACTIX_ONLINE_COMPLETED,capi.TACTIX_ONLINE_NO_CHANGE,
                            capi.TACTIX_ONLINE_SCOPED_FIXPOINT,capi.TACTIX_ONLINE_BUDGET_EXHAUSTED), (
                            call,event.base.status,event.base.step_id,event.base.rule_id,event.base.rule_result)
                    assert sequence.complete, ('custom SR did not finish',event.base.status,event.base.step_id,sequence.dispatches)
            finally:assert self.e.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_CONTEXT,0)>=0
        return 0


class HelpersProgram(CleanupProgram):
    helpers_only=True

# Known multiset fractions and explicit test policy thresholds. These are not
# a second copy of DefaultPolicy's defaults. Count calls to prove cheap rejection
# avoids the native census, while every observation preserves state and cache.
class CensusPolicy:
    def __init__(self, minimum, threshold):self.minimum,self.threshold=minimum,threshold
    def section(self, name):
        assert name=='rephase'
        return dict(configuration_checking_minimum_clauses=self.minimum,
                    complementary_ternary_minimum_permille=self.threshold)
for formula, expected in (([[1,2,3],[-1,-2,-3]],1000),
        ([[1,2,3],[1,2,3],[-1,-2,-3]],666),([[1,2],[2,3],[1,2,3]],0)):
    with Solver(library,default_frontend=True) as e:
        for clause in formula:e.add(clause)
        state=capi.tactix_walk_structure(binary_clauses=97)
        assert e.c.tactix_read_walk_structure(0,C.byref(state))==-1 and state.binary_clauses==97
        e.c.tactix_status()
        baseline=capi.tactix_strategy_state();context=capi.tactix_execution_state()
        assert not e.c.tactix_read_strategy_state(C.byref(baseline))
        assert not e.c.tactix_read_execution_state(C.byref(context))
        assert not e.c.tactix_read_walk_structure(1,C.byref(state))
        assert state.complementary_ternary_permille==expected,(formula,state.complementary_ternary_permille)
        calls=[];raw=e._lib.dll.tactix_read_walk_structure
        def counted(handle, census, out):
            calls.append(census);return raw(handle,census,out)
        e._lib.dll.tactix_read_walk_structure=counted
        try:
            assert not walk_preprocessing_deferred(e,CensusPolicy(100,500)) and calls==[0]
            calls.clear()
            assert walk_preprocessing_deferred(e,CensusPolicy(1,500))==(expected>=500)
            assert calls==([0,1] if state.large_clauses*1000//(state.large_clauses+state.binary_clauses)>=500 else [0])
            calls.clear()
            assert walk_preprocessing_deferred(e,CensusPolicy(len(formula),500))==(expected>=500)
            assert calls==([0,1] if state.large_clauses*1000//(state.large_clauses+state.binary_clauses)>=500 else [0])
            if expected==666:
                calls.clear();assert walk_preprocessing_deferred(e,CensusPolicy(3,666)) and calls==[0,1]
        finally:e._lib.dll.tactix_read_walk_structure=raw
        after=capi.tactix_strategy_state();after_context=capi.tactix_execution_state()
        assert not e.c.tactix_read_strategy_state(C.byref(after))
        assert not e.c.tactix_read_execution_state(C.byref(after_context))
        assert bytes(baseline)==bytes(after) and bytes(context)==bytes(after_context)
with Solver(library,default_frontend=True) as e:
    for offset in range(0,1650,3):
        e.add([offset+1,offset+2,offset+3]);e.add([-offset-1,-offset-2,-offset-3])
    with DefaultStartup(e) as startup:
        assert startup.preprocessing_deferred, 'native default census must classify paired ternaries'
models=proofs=prefixes=actions=authenticated=0
with tempfile.TemporaryDirectory(prefix='python-default-startup-') as raw:
    directory=Path(raw)
    for index,(n,formula,options) in enumerate(cases):
        cnf=directory/f'{index}.cnf'
        cnf.write_text(f'p cnf {n} {len(formula)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in formula))
        trajectory=None
        for mode,fmt in [('none',0),('auto',capi.TACTIX_PROOF_AUTO),('pbp',capi.TACTIX_PROOF_PBP)]:
            first,second=directory/f'native-{index}-{mode}.proof',directory/f'python-{index}-{mode}.proof'
            import os
            env=dict(os.environ,**options)
            native=subprocess.check_output([str(fixture),str(cnf),str(first),str(fmt)]+([
                'cleanup' if index in cleanup_cases else 'helpers' if index==helpers_case else 'full'] if full else []),text=True,env=env,timeout=90)
            expected=[line for line in native.splitlines() if line.startswith('action ')]
            answer=int(next(line.split()[1] for line in native.splitlines() if line.startswith('prefix ')))
            trace=[]
            with Solver(library,default_frontend=True) as s:
                if fmt:s.proof(second,fmt)
                s.read_dimacs(cnf)
                for name,value in options.items():assert not s.c.tactix_set_default_option(name.encode(),value.encode())
                raw_propagate=s._lib.dll.tactix_propagate
                def propagate(handle):
                    code=raw_propagate(handle)
                    trace.append(f'action Propagate {capi.TACTIX_INSTANCE_CANONICAL} {code}')
                    return code
                s._lib.dll.tactix_propagate=propagate
                original=s.apply
                def apply(rule,instance=None):
                    result=original(rule,instance)
                    trace.append(f'action {rule} {capi.TACTIX_INSTANCE_CANONICAL if instance is None else instance} {result}')
                    return result
                s.apply=apply
                def forbidden():raise AssertionError('startup must not hand off to native DefaultPolicy')
                s.default_solve=forbidden
                with DefaultStartup(s) as startup:
                    if index==deferred_case:assert startup.preprocessing_deferred
                    startup.original_input()
                    graph=next((list(map(int,line.split()[1:])) for line in native.splitlines() if line.startswith('graph ')),None)
                    if graph is not None:
                        state=capi.tactix_graph_labeling_state()
                        assert not s.c.tactix_read_graph_labeling_state(C.byref(state))
                        assert [state.authenticated,state.complete,state.fingerprint,state.continuation_work]==graph
                        authenticated+=bool(state.authenticated)
                    result=startup.constructors()
                    if full:
                        names={identity:name for name,identity in s._lib.rules.items()}
                        codes={0:0,1:1,2:2,3:10,4:20}
                        def observe(index,identity,event,*allowance):
                            r=event.base
                            rule_id=r.rule_id
                            if rule_id==2**32-1:
                                # Completion-only atomic advances have no raw
                                # rule id; native feedback names their authored
                                # step. Read the same definition, not the trace.
                                if hasattr(startup.structured,'rule_ids'):
                                    rule_id=startup.structured.rule_ids[identity]
                                else:
                                    with DefaultStage(s,startup.structured.family,index) as stage:
                                        rule_id=next(step.base.rule_id for step in stage.program.steps[:stage.program.step_count]
                                                     if step.base.step_id==identity)
                            trace.append(f'action {names[rule_id]} {r.instance} {codes[r.rule_result]}')
                        custom=(CleanupProgram if index in cleanup_cases else HelpersProgram if index==helpers_case else None)
                        with patch('tactix.structured.StructuredRoot',custom) if custom else nullcontext():
                            result=startup.structured_root(observer=observe)
                        try:startup.structured_root()
                        except RuntimeError:pass
                        else:raise AssertionError('structured startup opportunity repeated')
                        if index in cleanup_cases:
                            assert startup.structured_root_amo_binaries_added
                            assert sum(line.startswith('action BCE ') for line in trace)==1
                        if index==helpers_case:
                            assert startup.structured_root_amo_binaries_added
                            assert not any(line.startswith('action BCE ') for line in trace)
                        if not result:result=startup.early_lucky()
                        if not result:result=startup.attach_guidance()
                        if index==deferred_case:
                            assert result==0 and startup.structured is None
                    elif not result and startup.preprocessing_deferred:
                        result=startup.early_lucky()
                    assert result==answer,(index,mode,result,answer,native)
                    assert trace==expected,(index,mode,trace,expected)
                    if result==10:
                        model=set(s.model)
                        assert len(model)==n and all(any(lit in model for lit in clause) for clause in formula)
                        cpp=set(map(int,next(line[6:] for line in native.splitlines() if line.startswith('model ')).split())) if n else set()
                        assert model==cpp;models+=1
                    # The scope remains usable through caller-supplied phases.
                    if not result:
                        try:startup.original_input()
                        except RuntimeError:pass
                        else:raise AssertionError('original-input opportunity repeated')
                assert bool(s.c.tactix_stat(capi.TACTIX_STAT_HEURISTICS_ATTACHED))==bool(full and not result)
            s._lib.dll.tactix_propagate=raw_propagate
            assert trace==expected
            current=(answer,trace)
            if trajectory is None:trajectory=current
            else:assert current==trajectory,(index,mode,'proof mode changed startup trajectory')
            prefixes+=1;actions+=len(trace)
            if fmt:
                assert Path(str(first)+'.prefix').read_bytes()==second.read_bytes(),(index,mode)
                if result==20:
                    kind='pbp' if second.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
                    route='pbp' if kind=='pbp' else 'dsr-cake_lpr' if certificate_route(second)=='drat' else 'dsr-trestle'
                    checked=runner.check_proof(kind,str(cnf),str(second),30,route=route)
                    assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),checked
                    proofs+=1
    # Unwinding never leaves Startup tick attribution active.
    with Solver(library,default_frontend=True) as s:
        s.add([1,2]);s.add([-1,2])
        try:
            with DefaultStartup(s) as startup:
                startup.original_input()
                raise LookupError('caller phase failure')
        except LookupError:pass
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_CONTEXT,0)==-1
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,0)>=0
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_ATTACH_HEURISTICS,0)>=0
    # Refuse to pop a caller's still-open context; allow explicit recovery.
    with Solver(library,default_frontend=True) as s:
        s.add([1,2])
        startup=DefaultStartup(s);startup.__enter__()
        assert not s.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_CONTEXT,capi.TACTIX_CONTEXT_SPECULATION)
        try:startup.close()
        except RuntimeError:pass
        else:raise AssertionError('popped a foreign context')
        assert not s.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_CONTEXT,0)
        startup.close()
        state=capi.tactix_execution_state()
        assert not s.c.tactix_read_execution_state(C.byref(state)) and not state.context_depth
    # A cold Lucky model can be rewound at a new startup entry without H.
    with Solver(library,default_frontend=True) as s:
        s.add([1,2]);assert s.apply('Lucky',0)==10
        with DefaultStartup(s) as startup:
            assert not s.c.tactix_stat(capi.TACTIX_STAT_LEVEL)
            startup.original_input()
    # A phase opened by the caller is rejected before changing publication or
    # context ownership, so the caller can close it normally.
    with Solver(library) as s:
        s.add([1,2]);s.add([-1,2])
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,0)>=0
        startup=DefaultStartup(s)
        try:startup.__enter__()
        except RuntimeError:pass
        else:raise AssertionError('entered a foreign phase')
        assert s.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE,0)>=0
# Public-query candidates can satisfy F while falsifying assumptions. Keep
# this root strategy, explicitly rewind rejected candidates, then compose a
# Python search policy. No native DefaultPolicy handoff is involved.
rejected=[]
def query_startup(e, restored):
    startup=DefaultStartup(e)
    support=startup._support
    def observed_support(name,argument=0):
        code=support(name,argument)
        if name=='PREPARATION_RESTART':
            state=capi.tactix_execution_state()
            assert not e.c.tactix_read_execution_state(C.byref(state))
            assert not state.model_cached and not e.c.tactix_stat(capi.TACTIX_STAT_LEVEL)
        return code
    startup._support=observed_support
    with startup:
        assert e.c.tactix_config(capi.TACTIX_CFG_GUIDANCE_PUBLICATION)==capi.TACTIX_GUIDANCE_NATIVE
        startup.original_input();startup.constructors()
        startup.early_lucky();startup.attach_guidance()
        result=startup.result
    rejected.append(e.rejected_models)
    return result or ScriptedCDCL()(e,restored)
with Session(library,strategy=query_startup) as session:
    session.add([1,2])
    for _ in range(3):
        result=session.solve([-1]);assert result.status==10 and -1 in result.model and 2 in result.model
assert all(count>=1 for count in rejected),rejected
# Cancellation inside authentication is UNKNOWN, with balanced scope ownership;
# retry the same session through Python startup and Python search.
from tactix import _QueryInterrupted
cancel=[False]
interrupted=[]
def cancel_authentication(e, restored):
    apply=e.apply
    def interrupting(rule,instance=None):
        if not interrupted and rule=='Lucky' and instance==26:cancel[0]=True
        return apply(rule,instance)
    e.apply=interrupting
    try:
        with DefaultStartup(e) as startup:
            startup.original_input();startup.constructors();startup.early_lucky();startup.attach_guidance()
            result=startup.result
    except _QueryInterrupted:
        state=capi.tactix_execution_state()
        assert not e.c.tactix_read_execution_state(C.byref(state)) and not state.context_depth
        interrupted.append(True)
        raise
    return result or ScriptedCDCL()(e,restored)
with Session(library,strategy=cancel_authentication) as session:
    formula=graph_formula(4)
    for clause in formula:session.add(clause)
    session.set_terminate(lambda:cancel[0])
    assert session.solve().status==0 and interrupted
    cancel[0]=False
    result=session.solve();assert result.status==10
    assert all(any(lit in result.model for lit in clause) for clause in formula)

# Force termination at the last poll inside a refuting affine service. A
# first run measures the deterministic poll count; a fresh run cancels at that
# count, so the raw service refutes but its public query return is UNKNOWN.
affine_polls=[]
affine_returns=[]
affine_cancel=[False]
affine_active=[False]
affine_count=[0]
affine_threshold=[None]
def affine_terminate():
    if affine_active[0]:
        affine_count[0]+=1
        if affine_threshold[0] is not None and affine_count[0]>=affine_threshold[0]:
            affine_cancel[0]=True
    return affine_cancel[0]
def affine_query(e, restored):
    # Enable the structural service explicitly; plain Session views start
    # without provenance, while the standalone default frontend binds discard.
    assert not e.c.tactix_discard_structural_provenance()
    apply=e.apply
    def observed(rule,instance=None):
        profile,active=capi.tactix_online_affine_matrix_initialize_parameters(),C.c_int()
        search=False
        if rule=='AffineMatrixInitialize':
            assert not e.c.tactix_read_affine_matrix_profile(C.byref(profile),C.byref(active))
            search=active.value and profile.operation==capi.TACTIX_AFFINE_MATRIX_SEARCH_STEP
        affine_count[0]=0;affine_active[0]=bool(search)
        try:code=apply(rule,instance)
        finally:affine_active[0]=False
        if search:
            result=capi.tactix_affine_matrix_result()
            assert not e.c.tactix_read_affine_matrix_result(C.byref(result))
            if result.service_result==capi.TACTIX_AFFINE_RESULT_UNSAT:
                affine_polls.append(affine_count[0]);affine_returns.append(code)
        return code
    e.apply=observed
    with DefaultStartup(e) as startup:
        startup.original_input();startup.constructors();startup.early_lucky();startup.attach_guidance()
        result=startup.result
    return result or ScriptedCDCL()(e,restored)
affine_formula=[[1,2],[1,-2],[-1,2],[-1,-2]]
with Session(library,strategy=affine_query) as session:
    for clause in affine_formula:session.add(clause)
    session.set_terminate(affine_terminate)
    assert session.solve().status==20
assert len(affine_polls)==1 and affine_polls[0]>0 and affine_returns==[20],(affine_polls,affine_returns)
affine_threshold[0]=affine_polls[0]
with tempfile.TemporaryDirectory(prefix='affine-refutation-retry-') as raw:
    with Session(library,strategy=affine_query) as session:
        for clause in affine_formula:session.add(clause)
        session.set_terminate(affine_terminate)
        assert session.solve().status==0
        assert affine_returns[-1]==0 and affine_cancel[0],affine_returns
        affine_cancel[0]=False;affine_threshold[0]=None
        result=session.solve();assert result.status==20
        exported=Path(raw)/'retry';result.export(exported)
    proof=Path(str(exported)+'.proof');cnf=Path(str(exported)+'.cnf')
    kind='pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
    route='pbp' if kind=='pbp' else 'dsr-cake_lpr' if certificate_route(proof)=='drat' else 'dsr-trestle'
    checked=runner.check_proof(kind,str(cnf),str(proof),30,route=route)
    assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),checked
print({'affine_refutation_last_poll':affine_polls[0], 'masked_public_result':affine_returns[1], 'retry_proof':'VERIFIED'})
assert authenticated>=3,authenticated
polls=[0]
def census_terminate():polls[0]+=1;return False
def census_query(e,restored):
    before=polls[0]
    assert walk_preprocessing_deferred(e,CensusPolicy(1,1000))
    assert polls[0]==before,'census polled termination'
    return ScriptedCDCL()(e,restored)
with Session(library,strategy=census_query) as session:
    session.add([1,2,3]);session.add([-1,-2,-3]);session.set_terminate(census_terminate)
    result=session.solve();assert result.status==10
    assert any(v>0 for v in result.model) and any(v<0 for v in result.model)
print(f'Python-owned native startup prefix: {prefixes} comparisons, {actions} actions, {models} models, {proofs} proofs, {authenticated} authenticated graph snapshots; full controller remains open')
