"""Independent Python sequence policy versus the actual native controller."""
from pathlib import Path
import ctypes as C
import gc
import importlib.util
import os
import subprocess
import sys
import tempfile

library, fixture, root = map(Path, sys.argv[1:])
sys.path[:0] = [str(root/'python'), str(root/'tools/bench')]
from tactix import Solver, Session, ScriptedCDCL, capi, _QueryInterrupted
from tactix.sequence import OnlineSequence, strategy_state
from proof_oracle import certificate_route
from python_policy_reference import ScriptedDefault
spec = importlib.util.spec_from_file_location('sequence_oracles', root/'tools/bench/run.py')
runner = importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)


def request(execution, kind):
    steps = (capi.tactix_online_step_v2*2)()
    p, cleanup = steps[0].base, steps[1].base
    for i, (base, rule) in enumerate(((p,'SubstitutionRedundancy'),(cleanup,'Reduce')),1):
        base.step_id, base.rule_id = i, execution._lib.rules[rule]
        base.repeat_mode, base.repeat_limit = capi.TACTIX_ONLINE_REPEAT_ONCE, 1
        base.budget_unlimited = 1
    p.granularity = capi.TACTIX_ONLINE_GRANULARITY_ONE
    p.post_application = capi.TACTIX_ONLINE_POST_PROPAGATE
    clause = (C.c_int*1)(1)
    count = 2 if kind=='batch' else 1
    cubes = (capi.tactix_online_sr_instance*count)(
        *(capi.tactix_online_sr_instance(clause,1,None,0,None,0) for _ in range(count)))
    steps[0].sr_instances, steps[0].sr_instance_count = cubes, count
    if kind=='pool':
        p.granularity=capi.TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS
        steps[0].sr_instance_count=0;steps[0].sr_instances=None
        p.parameters.kind=capi.TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY
        p.parameters.value.substitution_redundancy=capi.tactix_online_sr_parameters(
            producer=capi.TACTIX_ONLINE_SR_GRAPH_SYMMETRY,
            maximum_detection_ticks=100000,maximum_validation_ticks=1024,
            maximum_graph_vertices=1024,maximum_graph_edges=4096,
            maximum_symmetry_generators=32,symmetry_backend_search_budget=10000,
            symmetry_backend_support_budget=100000,symmetry_backend_component_size_limit=1024,
            maximum_symmetry_fixing_instances=64,maximum_symmetry_generator_instances_per_pass=64,
            maximum_symmetry_group_ticks=100000,
            symmetry_fixing_mode=capi.TACTIX_ONLINE_SYMMETRY_STABILIZER_THEN_GENERATORS)
    cleanup.granularity = capi.TACTIX_ONLINE_GRANULARITY_CANONICAL
    cleanup.parameters.kind = capi.TACTIX_ONLINE_PARAMETERS_REDUCE
    # Explicit semantic fixture, not a second copy of DefaultPolicy settings.
    cleanup.parameters.value.reduce = capi.tactix_online_reduce_parameters(
        victim_numerator=0, victim_denominator=2, compact_collection_mode=capi.TACTIX_ONLINE_SETTING_ENABLE,
        reduction_bookkeeping_mode=capi.TACTIX_ONLINE_SETTING_DISABLE)
    options = dict(progress_followups=(2,))
    mode, limit = capi.TACTIX_ONLINE_REPEAT_ONCE, 1
    if kind=='before': options=dict(before_progress=(2,))
    if kind=='batch':
        p.granularity=capi.TACTIX_ONLINE_GRANULARITY_BATCH
        options['before_progress']=(1,)
        mode,limit=capi.TACTIX_ONLINE_REPEAT_FIXPOINT,0
    if kind in ('continue','external','stop','finish'):
        p.budget_unlimited,p.budget=0,0
        mode,limit=capi.TACTIX_ONLINE_REPEAT_COUNT,3
        options['on_budget_exhaustion']={'stop':capi.TACTIX_SEQUENCE_BUDGET_STOP,
            'finish':capi.TACTIX_SEQUENCE_BUDGET_FINISH_ROUND}.get(kind,capi.TACTIX_SEQUENCE_BUDGET_CONTINUE)
    if kind=='retain':
        p.budget_unlimited,p.budget=0,1
        mode,limit=capi.TACTIX_ONLINE_REPEAT_COUNT,2
    return capi.tactix_online_program_v2(steps,2,mode,limit), options


def row(result, sequence, pool):
    r=result.base
    values=[r.status,r.rule_result,r.budget_status,r.work_unit,r.work,r.applications,r.step_id,r.rule_id,
        r.instance,r.made_rule_progress,r.selection_complete,r.configuration_revision,
        r.before_arena_generation,r.before_touch_epoch,r.before_strategy_epoch,
        r.after_arena_generation,r.after_touch_epoch,r.after_strategy_epoch,
        result.discovery_progress_units,result.discovery_entry_required,result.discovery_exhausted,
        sequence.rounds,int(sequence.complete)]
    for i, dispatches in enumerate(sequence.dispatches): values.extend((dispatches,sequence.consumed(i)))
    values.extend((pool.limit,pool.consumed,pool.unlimited,pool.accounting_overflow) if pool is not None else (0,0,1,0))
    return 'row '+' '.join(map(str,values))


def relative_versions(rows):
    # Attaching a proof is an explicit public setup transition. Compare every
    # execution-version increment across formats, not that initial epoch offset.
    values=[list(map(int,line.split()[1:])) for line in rows]
    if values:
        initial=values[0][12:15]
        for value in values:
            for coordinate in range(3):
                value[12+coordinate]-=initial[coordinate]
                value[15+coordinate]-=initial[coordinate]
    return values


def python_run(kind, cnf, proof, fmt, finish):
    trace=[]
    with Solver(library) as execution:
        if fmt: execution.proof(proof,fmt)
        execution.read_dimacs(cnf)
        verdict=20 if execution.c.tactix_status()==20 else 0
        if not verdict:
            verdict=execution.c.tactix_propagate()
            assert verdict in (0,20)
        if not verdict:
            for context in (capi.TACTIX_CONTEXT_STARTUP,capi.TACTIX_CONTEXT_INPROCESSING):
                assert execution.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_CONTEXT,context)>=0
            program,options=request(execution,kind)
            pool=(capi.tactix_online_discovery_budget(limit=0,ticks_per_generator=1024,hard_limit=20000000)
                  if kind=='pool' else None)
            with OnlineSequence(execution,program,revision=7,**options) as sequence:
                # Every payload is now owned; modifying the source cannot alter
                # a later rearmed round. Retain case explicitly reconfigures.
                if kind!='retain':
                    program.steps[0].base.rule_id=-1
                    program.repeat_mode=999
                    del program;gc.collect()
                call=0
                while not sequence.complete:
                    assert call<2000
                    result=sequence.advance(pool);trace.append(row(result,sequence,pool))
                    if pool is not None and (result.discovery_progress_units or result.discovery_entry_required):
                        pool.limit=max(pool.limit,pool.consumed)+1024
                    if result.base.status in (capi.TACTIX_ONLINE_SAT,capi.TACTIX_ONLINE_UNSAT):
                        verdict=10 if result.base.status==capi.TACTIX_ONLINE_SAT else 20
                        break
                    if kind=='external' and not call: assert execution.c.tactix_propagate()>=0
                    if kind=='retain' and not call:
                        reverse=(capi.tactix_online_step_v2*2)(program.steps[1],program.steps[0])
                        replacement=capi.tactix_online_program_v2(reverse,2,program.repeat_mode,program.repeat_limit)
                        sequence.configure(replacement,revision=8,mode=capi.TACTIX_ONLINE_RECONFIGURE_RETAIN_BUDGETS,**options)
                    if kind=='stop' and call==2: break
                    assert result.base.status in (capi.TACTIX_ONLINE_COMPLETED,capi.TACTIX_ONLINE_NO_CHANGE,
                        capi.TACTIX_ONLINE_SCOPED_FIXPOINT,capi.TACTIX_ONLINE_BUDGET_EXHAUSTED)
                    call+=1
            if kind=='pool':
                assert pool.consumed>0 and any(int(item.split()[20]) for item in trace)
            for context in (capi.TACTIX_CONTEXT_INPROCESSING,capi.TACTIX_CONTEXT_STARTUP):
                assert execution.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_CONTEXT,0)>=0
        prefix=verdict
        if finish and not verdict:
            execution.default_solve=lambda: (_ for _ in ()).throw(AssertionError('no native DefaultPolicy handoff'))
            verdict=ScriptedDefault().run(execution.c,execution._lib.rules)
        if verdict==20: assert execution.c.tactix_status()==20
        model=execution.model if verdict==10 else ()
    return trace,prefix,verdict,model


def graph(n):
    cell=lambda a,b:a*n+b+1
    return [[cell(a,b) for a in range(n)] for b in range(n)]+[
        [-cell(a,b),-cell(a,c)] for a in range(n) for b in range(n) for c in range(b+1,n)]+[
        [-cell(a,0),-cell(b,0)] for a in range(n) for b in range(a+1,n)]


normal=[[-1,2,3],[1,2],[1,-2]]
unsat=[[1,2],[1,-2],[-1,2],[-1,-2]]
cases=[(kind,normal) for kind in ('manual','before','batch','continue','stop','finish','retain','external')]
cases += [('manual',normal+[[1]]),('manual',unsat),('pool',graph(4))]
if str(library)!='-':
    for version in range(1,18):
        with Solver(library) as execution:
            assert execution.c.tactix_status() in (0,10)

            assert execution.c.tactix_propagate()==0
            suffix=f'_v{version}' if version>1 else ''
            record=getattr(capi,'tactix_online_step'+suffix)()
            base=record
            while hasattr(base,'base'):base=base.base
            base.step_id,base.rule_id=31,execution._lib.rules['Propagate']
            base.repeat_mode,base.repeat_limit=capi.TACTIX_ONLINE_REPEAT_ONCE,1
            base.granularity=capi.TACTIX_ONLINE_GRANULARITY_CANONICAL
            base.post_application=capi.TACTIX_ONLINE_POST_PROPAGATE
            base.work_unit=capi.TACTIX_ONLINE_WORK_PROPAGATIONS
            base.budget_unlimited=1
            steps=(type(record)*1)(record)
            program=getattr(capi,'tactix_online_program'+suffix)(steps,1,capi.TACTIX_ONLINE_REPEAT_COUNT,2)
            with OnlineSequence(execution,program,version=version) as sequence:
                C.memset(C.byref(steps),0,C.sizeof(steps))
                program.repeat_mode=999
                rejected_flags=0
                for call in range(8):
                    raw_advance=execution._lib.dll.tactix_online_executor_advance
                    def flag_candidate(handle,executor,pool,out):
                        code=raw_advance(handle,executor,pool,out)
                        if not code:
                            C.cast(out,C.POINTER(capi.tactix_online_execution_result)).contents.query_rejected_model=1
                        return code
                    # Protocol fault injection only: a candidate-rejection bit
                    # must never relabel this real nonterminal event as SAT.
                    if version==2: execution._lib.dll.tactix_online_executor_advance=flag_candidate
                    try: result=sequence.advance()
                    finally: execution._lib.dll.tactix_online_executor_advance=raw_advance
                    rejected_flags+=bool(result.query_rejected_model)
                    assert result.base.status in (capi.TACTIX_ONLINE_COMPLETED,capi.TACTIX_ONLINE_NO_CHANGE)
                    if sequence.complete:break
                else:raise AssertionError(('version did not complete',version))
                # Each native atomic pass returns its application first, then
                # completes on a separate advance (finish_step_pass/sequence).
                assert sequence.rounds==2 and sequence.dispatches==[4],(version,sequence.rounds,sequence.dispatches)
                if version==2:assert rejected_flags==4
                before=(sequence.rounds,sequence.dispatches[:],sequence.consumed(0))
                program.step_count=-1
                try:sequence.configure(program,version=version)
                except ValueError:pass
                else:raise AssertionError('invalid replacement accepted')
                assert (sequence.rounds,sequence.dispatches,sequence.consumed(0))==before
                saved=sequence.advance();saved.base.status=999
                assert sequence.advance().base.status==capi.TACTIX_ONLINE_COMPLETED
    with Solver(library) as execution:
        with OnlineSequence(execution) as sequence:
            assert sequence.advance().base.status==capi.TACTIX_ONLINE_INVALID_REQUEST
            empty=capi.tactix_online_program_v2(None,0,capi.TACTIX_ONLINE_REPEAT_ONCE,1)
            for options in ({'version':0},{'version':2.0},{'revision':-1},{'mode':9},
                    {'on_budget_exhaustion':9},{'progress_followups':(1,)},{'before_progress':(1,)}):
                try:sequence.configure(empty,**options)
                except ValueError:pass
                else:raise AssertionError(options)
            assert not execution.c.tactix_stat(capi.TACTIX_STAT_INPUT_CLOSED)
            sequence.configure(empty)
            assert sequence.advance().base.status==capi.TACTIX_ONLINE_COMPLETED and sequence.complete
comparisons=models=proofs=advances=0
with tempfile.TemporaryDirectory(prefix='python-sequence-') as raw:
    directory=Path(raw)
    for case,(kind,formula) in enumerate(cases):
        cnf=directory/f'{case}.cnf';n=max(abs(l) for c in formula for l in c)
        cnf.write_text(f'p cnf {n} {len(formula)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in formula))
        trajectory=None
        for label,fmt in [('none',0),('auto',capi.TACTIX_PROOF_AUTO),('pbp',capi.TACTIX_PROOF_PBP)]:
            for finish in (0,1):
                native_proof=directory/f'{case}-{label}-{finish}-native.proof'
                python_proof=directory/f'{case}-{label}-{finish}-python.proof'
                output=subprocess.check_output([str(fixture),kind,str(cnf),str(native_proof),str(fmt),str(finish)],text=True,timeout=90)
                expected=[line for line in output.splitlines() if line.startswith('row ')]
                prefix=int(next(line.split()[1] for line in output.splitlines() if line.startswith('prefix ')))
                verdict=int(next(line.split()[1] for line in output.splitlines() if line.startswith('verdict ')))
                if finish==0:
                    current=(prefix,relative_versions(expected))
                    if case==0 and expected:
                        print('Native proof setup:',label,'versions',expected[0].split()[13:16],flush=True)
                    if trajectory is None: trajectory=current
                    else: assert current==trajectory,(kind,label,'proof mode changes sequence')
                artifacts=[(native_proof,verdict)]
                if str(library)!='-':
                    actual,actual_prefix,answer,model=python_run(kind,cnf,python_proof,fmt,finish)
                    assert actual==expected,(kind,label,finish,'first different advance',actual,expected)
                    assert actual_prefix==prefix and (not finish or answer in (10,20))
                    assert not finish or answer==verdict
                    if not finish and fmt: assert native_proof.read_bytes()==python_proof.read_bytes(),(kind,label,'prefix proof')
                    if answer==10:
                        values=set(model)
                        assert len(model)==n and all(any(l in values for l in c) for c in formula)
                        models+=1
                    artifacts.append((python_proof,answer))
                    comparisons+=1;advances+=len(actual)
                if verdict==10:
                    model=set(map(int,next(line.split()[1:] for line in output.splitlines() if line.startswith('model'))))
                    assert len(model)==n and all(any(l in model for l in c) for c in formula);models+=1
                for path,answer in artifacts:
                    if fmt and answer==20:
                        form='pbp' if path.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
                        route='pbp' if form=='pbp' else 'dsr-cake_lpr' if certificate_route(path)=='drat' else 'dsr-trestle'
                        checked=runner.check_proof(form,str(cnf),str(path),30,route=route)
                        assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),checked
                        proofs+=1
print(f'Python/native sequence: {comparisons} comparisons, {advances} advances, {models} checked models, {proofs} checked proofs')

if str(library)!='-':
    rejections=[];polls=[0];cancel=[False];cancelled=[];interruptions=[];cancel_results=[]
    def terminate():
        polls[0]+=1
        return cancel[0]
    def query_sequence(execution,restored):
        before=polls[0]
        for _ in range(12):
            observed=strategy_state(execution)
            assert observed.query_status in (capi.TACTIX_EXECUTION_COMPLETE,capi.TACTIX_EXECUTION_UNKNOWN)
        assert polls[0]==before,'pure strategy observations polled termination'
        empty=capi.tactix_online_program_v2(None,0,capi.TACTIX_ONLINE_REPEAT_ONCE,1)
        with OnlineSequence(execution,empty) as idle:
            before=polls[0]
            assert idle.advance().base.status==capi.TACTIX_ONLINE_COMPLETED
            assert idle.advance().base.status==capi.TACTIX_ONLINE_COMPLETED
            assert polls[0]==before,'sequence boundary polled termination or reconstructed a model'
        assert not execution.c.tactix_set_guidance_publication(capi.TACTIX_GUIDANCE_NATIVE)
        step=capi.tactix_online_step_v2()
        step.base.step_id,step.base.rule_id=41,execution._lib.rules['Lucky']
        step.base.granularity=capi.TACTIX_ONLINE_GRANULARITY_CANONICAL
        step.base.repeat_mode,step.base.repeat_limit=capi.TACTIX_ONLINE_REPEAT_ONCE,1
        step.base.budget_unlimited=1
        program=capi.tactix_online_program_v2(C.pointer(step),1,capi.TACTIX_ONLINE_REPEAT_ONCE,1)
        raw_advance=execution._lib.dll.tactix_online_executor_advance
        def interrupting(handle,executor,pool,out):
            first=cancel_test[0] and not cancelled
            if first:
                cancelled.append(True);cancel[0]=True
            code=raw_advance(handle,executor,pool,out)
            if first:
                result=C.cast(out,C.POINTER(capi.tactix_online_execution_result)).contents
                state=strategy_state(execution)
                cancel_results.append((code,result.base.status,result.base.rule_result,state.query_status,state.stopped))
            return code
        execution._lib.dll.tactix_online_executor_advance=interrupting
        before_rejected=execution.rejected_models
        try:
            with OnlineSequence(execution,program) as sequence:
                for _ in range(8):
                    result=sequence.advance()
                    if result.query_rejected_model:
                        assert result.base.status not in (capi.TACTIX_ONLINE_SAT,capi.TACTIX_ONLINE_UNSAT)
                        assert execution.c.tactix_support(capi.TACTIX_SUPPORT_PREPARATION_RESTART,0)>=0
                        break
                    if result.base.status in (capi.TACTIX_ONLINE_SAT,capi.TACTIX_ONLINE_UNSAT):
                        return 10 if result.base.status==capi.TACTIX_ONLINE_SAT else 20
                    if sequence.complete:break
                else:raise AssertionError('query sequence did not finish')
        except _QueryInterrupted:
            state=strategy_state(execution)
            assert state.stopped or state.query_status==capi.TACTIX_EXECUTION_INTERRUPTED
            interruptions.append(True)
            raise
        finally:
            execution._lib.dll.tactix_online_executor_advance=raw_advance
        count=execution.rejected_models-before_rejected
        if count:
            assert execution.c.tactix_stat(capi.TACTIX_STAT_LEVEL)==0
        rejections.append(count)
        return ScriptedCDCL()(execution,restored)
    cancel_test=[False]
    with Session(library,strategy=query_sequence) as session:
        session.add([1,2]);session.set_terminate(terminate)
        for _ in range(3):
            result=session.solve([-1]);assert result.status==10 and {-1,2}<=set(result.model)
    assert sum(rejections)>0,rejections
    cancel_test[0]=True
    with tempfile.TemporaryDirectory(prefix='sequence-query-retry-') as raw:
        with Session(library,strategy=query_sequence) as session:
            for clause in unsat:session.add(clause)
            session.set_terminate(terminate)
            assert session.solve().status==0 and interruptions and cancelled
            assert cancel_results==[(0,capi.TACTIX_ONLINE_NO_CHANGE,capi.TACTIX_RULE_RESULT_NO_OP,
                capi.TACTIX_EXECUTION_INTERRUPTED,1)],cancel_results
            cancel[0]=False
            result=session.solve();assert result.status==20
            exported=Path(raw)/'retry';result.export(exported)
        proof=Path(str(exported)+'.proof');cnf=Path(str(exported)+'.cnf')
        form='pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
        route='pbp' if form=='pbp' else 'dsr-cake_lpr' if certificate_route(proof)=='drat' else 'dsr-trestle'
        checked=runner.check_proof(form,str(cnf),str(proof),30,route=route)
        assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),checked
    print('Real query sequence:',rejections,'rejections; no-poll boundaries; cancellation result',cancel_results,
          'UNKNOWN and proof-checked retry pass')
