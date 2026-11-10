"""Python root scheduling versus DefaultInprocessScheduler itself."""
from pathlib import Path
import ctypes as C
import importlib.util
import os
import subprocess
import sys
import tempfile

library, fixture, root = map(Path, sys.argv[1:])
sys.path[:0] = [str(root/'python'), str(root/'tools/bench')]
from tactix import Solver, Session, ScriptedCDCL, capi
from tactix.structured import StructuredRoot
from tactix.sequence import strategy_state
from proof_oracle import certificate_route
from python_policy_reference import ScriptedDefault
spec = importlib.util.spec_from_file_location('root_oracles', root/'tools/bench/run.py')
runner = importlib.util.module_from_spec(spec); spec.loader.exec_module(runner)


def telemetry(label, t):
    return label+' '+' '.join(str(int(getattr(t, key))) for key in (
        'runs', 'advances', 'applications', 'effective', 'sequence_passes', 'terminal'))


def python_run(cnf, proof, fmt, arm, finish=False, observed=True):
    with Solver(library, default_frontend=True) as e:
        if fmt: e.proof(proof, fmt)
        e.read_dimacs(cnf)
        assert e.c.tactix_status()!=20
        assert e.c.tactix_support(capi.TACTIX_SUPPORT_MATERIALIZE_COLD_STATE, 0)>=0
        assert e.c.tactix_propagate()==0
        assert e.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_CONTEXT, capi.TACTIX_CONTEXT_STARTUP)>=0
        if arm==6: assert not e.c.tactix_support(capi.TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,0)
        config=e.default_definition.section('inprocess')
        if arm==1: config['structured_root_backtracking_domain_limit']=0
        if arm==2: config['structured_root_large_domain_limit']=0
        if arm in (3,4):
            config.update(structured_root_progress_minimum_ticks=1,
                structured_root_progress_ticks_per_variable=0,
                structured_root_sr_discovery_unlimited=0,
                structured_root_sr_discovery_ticks=256 if arm==3 else 0)
        if arm==5: config['structured_root_progress_feedback']=0
        if arm in (7,8):
            config.update(structured_root_sr_discovery_unlimited=1,
                structured_root_progress_feedback=int(arm==8),
                structured_root_progress_minimum_ticks=1,
                structured_root_progress_ticks_per_variable=0)
        if arm==9:
            config.update(structured_root_large_domain_limit=0,
                structured_root_large_domain_progress_feedback=1,
                structured_root_progress_minimum_ticks=1,
                structured_root_progress_ticks_per_variable=0,
                structured_root_sr_discovery_unlimited=0,
                structured_root_sr_discovery_ticks=256)
        if arm==10: config['structured_root_progress_ticks_per_variable']=2**64-1
        actions=[]
        def observe(index, identity, event, unlimited, allowance, consumed, granted):
            r=event.base
            description=(f'structured-stage={index};revision=1;step={identity};'
                         f'discovery-consumed={consumed};discovery-limit={granted}')
            actions.append('action '+description+' '+' '.join(map(str, (
                int(unlimited),allowance,r.rule_result,r.work_unit,r.work,r.applications,
                r.made_rule_progress,event.discovery_progress_units,
                event.discovery_entry_required,event.discovery_exhausted))))
        controller=StructuredRoot(e, config=config, observer=observe if observed else None)
        initial=strategy_state(e)
        answer=controller.run()
        t=controller.telemetry; o=controller.ownership
        rows=['initial '+' '.join(map(str,(initial.arena_generation,initial.touch_epoch,
            initial.strategy_epoch)))]+actions+[telemetry('total', t), 'discovery '+' '.join(map(str, (
            t.sr_discovery_ticks, t.sr_discovery_progress_window,
            t.sr_discovery_renewals, int(t.sr_discovery_overflow))))]
        rows.extend(telemetry('stage', stage) for stage in t.stages)
        rows.append('ownership '+' '.join(map(str, (int(o.grew_root_trail), o.external_begin,
            o.external_end, int(o.symmetry_applied), len(o.coupled_factor_ranges)))))
        state=strategy_state(e)
        rows.append('state '+' '.join(str(getattr(state, key)) for key in (
            'arena_generation', 'touch_epoch', 'strategy_epoch', 'trail_size', 'sr_applied',
            'unique_literal_amo_binaries', 'nonforcing_cuts_declined',
            'post_amo_nonhelper_applied', 'post_amo_graph_root_units')))
        assert controller.consumed and not controller._running
        assert controller.run()==0
        after=strategy_state(e)
        assert bytes(after)==bytes(state), 'consumed root changed kernel state'
        context=capi.tactix_execution_state()
        assert not e.c.tactix_read_execution_state(C.byref(context))
        assert context.context_depth==1 and context.context_top==capi.TACTIX_CONTEXT_STARTUP
        phase=capi.tactix_inprocess_state()
        assert not e.c.tactix_read_inprocess_state(C.byref(phase))
        assert bool(phase.phase_open)==(arm==6) and not phase.occurrences_open
        if arm==6: assert e.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE,0)>=0
        assert e.c.tactix_support(capi.TACTIX_SUPPORT_LEAVE_CONTEXT, 0)>=0
        rows.append('result '+str(answer))
        if finish and not answer: answer=ScriptedDefault().run(e.c,e._lib.rules)
        return rows, answer, e.model if answer==10 else ()


def graph(n):
    cell=lambda a,b:a*n+b+1
    return [[cell(a,b) for a in range(n)] for b in range(n)]+[
        [-cell(a,b),-cell(a,c)] for a in range(n) for b in range(n) for c in range(b+1,n)]+[
        [-cell(a,0),-cell(b,0)] for a in range(n) for b in range(a+1,n)]


def pigeonhole(p,h):
    cell=lambda a,b:a*h+b+1
    return [[cell(a,b) for b in range(h)] for a in range(p)]+[
        [-cell(a,b),-cell(c,b)] for b in range(h) for a in range(p) for c in range(a+1,p)]


if str(library)!='-':
    with Solver(library,default_frontend=True) as e:
        controller=StructuredRoot(e)
        before=strategy_state(e)
        try: controller.run()
        except RuntimeError: pass
        else: raise AssertionError('open input accepted')
        assert not controller.consumed and bytes(strategy_state(e))==bytes(before)
        for bad in (-1,2**64,True,1.0,None):
            config=e.default_definition.section('inprocess')
            config['structured_root_sr_discovery_ticks']=bad
            try: StructuredRoot(e,config=config)
            except ValueError: pass
            else: raise AssertionError(('invalid work bound accepted',bad))
        config=e.default_definition.section('inprocess')
        stable=StructuredRoot(e,config=config)
        original=stable.config['structured_root_sr_discovery_ticks']
        config['structured_root_sr_discovery_ticks']=-1
        assert stable.config['structured_root_sr_discovery_ticks']==original
        try: stable.config={}
        except AttributeError: pass
        else: raise AssertionError('settings rebound')
        try: stable.config['structured_root_sr_discovery_ticks']=-1
        except TypeError: pass
        else: raise AssertionError('settings mutated')
        assert not e.c.tactix_stat(capi.TACTIX_STAT_INPUT_CLOSED)
        for clause in ([1,2],[-1,2]): e.add(clause)
        e.c.tactix_status()
        assert e.c.tactix_support(capi.TACTIX_SUPPORT_MATERIALIZE_COLD_STATE,0)>=0
        assert e.c.tactix_propagate()==0
        interrupted=StructuredRoot(e)
        support=interrupted._support
        def interrupted_entry(name,argument=0):
            support(name,argument)
            if name=='ENTER_CONTEXT': raise KeyboardInterrupt()
        interrupted._support=interrupted_entry
        try: interrupted.run()
        except KeyboardInterrupt: pass
        else: raise AssertionError('entry interrupt not injected')
        context=capi.tactix_execution_state()
        assert not e.c.tactix_read_execution_state(C.byref(context)) and not context.context_depth
        def fail_observer(*args):
            try: controller.run()
            except RuntimeError: pass
            else: raise AssertionError('controller re-entered')
            try: controller.close()
            except RuntimeError: pass
            else: raise AssertionError('observer closed the running controller')
            raise ValueError('observer failure')
        controller=StructuredRoot(e,observer=fail_observer)
        try: controller.run()
        except ValueError as error: assert str(error)=='observer failure'
        else: raise AssertionError('observer did not dispatch')
        state=capi.tactix_execution_state()
        assert not e.c.tactix_read_execution_state(C.byref(state))
        assert not state.context_depth and not state.inprocess_phase_open
        assert controller.consumed and not controller._running and controller.run()==0


comparisons=models=proofs=renewals=discovery_ticks=0
with tempfile.TemporaryDirectory(prefix='structured-root-') as raw:
    directory=Path(raw)
    for case, formula in enumerate(([[1,2,3],[-1,-2,3],[-1,2,-3],[1,-2,-3]],
            [[-1,2,3],[1,2],[1,-2]],
            [[1,2],[1,-2],[-1,2],[-1,-2]], graph(4),pigeonhole(5,4))):
        if case==0: os.environ.pop('TACTIX_NO_AFFINE_MODEL',None)
        else: os.environ['TACTIX_NO_AFFINE_MODEL']='1'
        n=max(abs(l) for c in formula for l in c)
        cnf=directory/f'{case}.cnf'
        cnf.write_text(f'p cnf {n} {len(formula)}\n'+''.join(
            ' '.join(map(str,c))+' 0\n' for c in formula))
        for arm in range(11):
            trajectory=None
            for fmt in (0,capi.TACTIX_PROOF_AUTO,capi.TACTIX_PROOF_PBP):
                native=directory/f'{case}-{arm}-{fmt}-native.proof'
                python=directory/f'{case}-{arm}-{fmt}-python.proof'
                output=subprocess.check_output([str(fixture),str(cnf),str(native),str(fmt),str(arm)],
                                               text=True,timeout=90)
                rows=[line for line in output.splitlines() if line.startswith(
                    ('initial ','action ','total ','discovery ','stage ','ownership ','state ','result '))]
                answer=int(rows[-1].split()[1]); artifacts=[(native,answer)]
                discovery=next(line.split() for line in rows if line.startswith('discovery '))
                discovery_ticks+=int(discovery[1]); renewals+=int(discovery[3])
                initial=list(map(int,rows[0].split()[1:]))
                normalized=[]
                for line in rows:
                    fields=line.split()
                    if fields[0] in ('initial','state'):
                        fields[1:4]=[str(int(fields[1+i])-initial[i]) for i in range(3)]
                        line=' '.join(fields)
                    normalized.append(line)
                if trajectory is None: trajectory=normalized
                else: assert trajectory==normalized, (case,arm,fmt,'proof-dependent schedule')
                if str(library)!='-':
                    actual, actual_answer, model=python_run(cnf,python,fmt,arm)
                    assert actual==rows,(case,arm,fmt,actual,rows)
                    assert actual_answer==answer
                    if fmt: assert native.read_bytes()==python.read_bytes(),(case,arm,fmt,'prefix proof')
                    artifacts.append((python,answer)); comparisons+=1
                    noobserver=python.with_suffix('.noobserver')
                    quiet,quiet_answer,quiet_model=python_run(cnf,noobserver,fmt,arm,observed=False)
                    assert quiet==[line for line in rows if not line.startswith('action ')]
                    assert quiet_answer==answer and quiet_model==model
                    if fmt: assert noobserver.read_bytes()==python.read_bytes()
                    comparisons+=1
                    if answer==10:
                        assert len(model)==n and all(any(l in model for l in c) for c in formula)
                        models+=1
                if answer==10:
                    model=tuple(map(int,next(line.split()[1:] for line in output.splitlines()
                                            if line.startswith('model '))))
                    assert len(model)==n and all(any(l in model for l in c) for c in formula)
                    models+=1
                for path, result in artifacts:
                    if fmt and result==20:
                        form='pbp' if path.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
                        route='pbp' if form=='pbp' else 'dsr-cake_lpr' if certificate_route(path)=='drat' else 'dsr-trestle'
                        checked=runner.check_proof(form,str(cnf),str(path),30,route=route)
                        assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),checked
                        proofs+=1
print(f'Structured root: {comparisons} native/Python comparisons, {models} models, {proofs} proofs')
assert discovery_ticks and renewals, 'shared discovery and outer renewal must be reached'
print(f'Discovery coverage: {discovery_ticks} ticks, {renewals} outer renewals')

# Complete SAT and UNSAT transformations independently. This is an artifact
# oracle after the root component, not full DefaultPolicy correspondence.
with tempfile.TemporaryDirectory(prefix='structured-completion-') as raw:
    directory=Path(raw)
    for case,formula in enumerate(([[-1,2,3],[1,2],[1,-2]],graph(3),graph(4),pigeonhole(5,4))):
        n=max(abs(l) for c in formula for l in c)
        cnf=directory/f'{case}.cnf'
        cnf.write_text(f'p cnf {n} {len(formula)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in formula))
        for arm in (0,2,3,4):
            for fmt in (0,capi.TACTIX_PROOF_AUTO,capi.TACTIX_PROOF_PBP):
                proof=directory/f'{case}-{arm}-{fmt}.proof'
                output=subprocess.check_output([str(fixture),str(cnf),str(proof),str(fmt),str(arm),'1'],text=True,timeout=90)
                answer=int(next(line.split()[1] for line in output.splitlines() if line.startswith('verdict ')))
                assert answer in (10,20)
                artifacts=[(proof,answer)]
                if answer==10:
                    model=tuple(map(int,next(line.split()[1:] for line in output.splitlines() if line.startswith('model '))))
                    assert len(model)==n and all(any(l in model for l in c) for c in formula)
                    models+=1
                if str(library)!='-':
                    python=proof.with_suffix('.python')
                    _,actual,model=python_run(cnf,python,fmt,arm,True)
                    assert actual==answer
                    artifacts.append((python,actual))
                    if actual==10:
                        assert len(model)==n and all(any(l in model for l in c) for c in formula)
                        models+=1
                for path,result in artifacts:
                    if fmt and result==20:
                        form='pbp' if path.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
                        route='pbp' if form=='pbp' else 'dsr-cake_lpr' if certificate_route(path)=='drat' else 'dsr-trestle'
                        checked=runner.check_proof(form,str(cnf),str(path),30,route=route)
                        assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),checked
                        proofs+=1
print(f'With independent artifact completion: {models} models, {proofs} proofs')
assert models and proofs, 'original-input model and proof oracles must run'

if str(library)!='-':
    os.environ.pop('TACTIX_NO_AFFINE_MODEL',None)
    rejected=[]
    def query_root(e,restored):
        before=e.rejected_models
        trail_before=strategy_state(e).trail_size
        candidate=[]
        def observe(index,identity,event,*allowance):
            if event.query_rejected_model:
                candidate.append(strategy_state(e).trail_size)
        controller=StructuredRoot(e,observer=observe)
        answer=controller.run()
        if e.rejected_models>before:
            assert not answer
            if controller.query_rejected_model:
                assert candidate and controller.ownership.grew_root_trail==(candidate[-1]>trail_before)
                assert controller.telemetry.terminal==capi.TACTIX_ONLINE_SAT
                assert e.c.tactix_support(capi.TACTIX_SUPPORT_PREPARATION_RESTART,0)>=0
            else:
                # Native OnlineStrategyExecutor admits query candidates and
                # restarts rejected ones inside its version transaction.
                # Such a rejection is not the C wrapper's later SAT mask.
                assert not candidate
                assert controller.ownership.grew_root_trail==(strategy_state(e).trail_size>trail_before)
            assert not e.c.tactix_stat(capi.TACTIX_STAT_LEVEL)
            state=capi.tactix_execution_state()
            assert not e.c.tactix_read_execution_state(C.byref(state)) and not state.model_cached
            rejected.append(True)
        return answer or ScriptedCDCL()(e,restored)
    with tempfile.TemporaryDirectory(prefix='structured-query-') as raw:
        with Session(library,strategy=query_root) as session:
            for clause in ([1,2,3],[-1,-2,3],[-1,2,-3],[1,-2,-3]):session.add(clause)
            result=session.solve([-1,-2,-3]);assert result.status==20
            exported=Path(raw)/'query';result.export(exported)
        proof=Path(str(exported)+'.proof');cnf=Path(str(exported)+'.cnf')
        form='pbp' if proof.read_bytes().startswith(b'pseudo-Boolean') else 'dsr'
        route='pbp' if form=='pbp' else 'dsr-cake_lpr' if certificate_route(proof)=='drat' else 'dsr-trestle'
        checked=runner.check_proof(form,str(cnf),str(proof),30,route=route)
        assert checked[:3]==(True,'VERIFIED',runner.CHK_VERIFIED),checked
    assert rejected,'structured affine candidate rejection was not reached'
    print('Real structured query candidate rejection: clean root/cache, ownership, checked query proof')
