"""Native solve-entry configuration versus public C/Python definition reads."""
from pathlib import Path
import ctypes as C
import os
import gc
import weakref
import subprocess
import sys
library, fixture, observations, root = map(Path, sys.argv[1:])
sys.path[:0] = [str(root/'python'), str(root/'tests')]
from tactix import Solver, Session, ScriptedCDCL, capi
from tactix.definition import StrategyDefinition
from tactix.program import DefaultStage
from python_policy_reference import NativeInprocessSchedule
import json
native_config=json.loads(subprocess.check_output([str(observations)],text=True).splitlines()[0])['config']
# Session-local settings must override the environment, just as native solve does.
os.environ['TACTIX_FORCE_MODE']='focused'
ordinary=subprocess.check_output([str(fixture)],text=True).splitlines()
query=subprocess.check_output([str(fixture),'--query'],text=True).splitlines()
assert len(ordinary)==len(query)==3 and ordinary[0]!=query[0]
stage_rows=subprocess.check_output([str(fixture),'--stage-feedback'],text=True).splitlines()
with Solver(library,default_frontend=True) as owner:
    for row in stage_rows:
        family,index,flags,*expected=row.split()
        family,index,flags=map(int,(family,index,flags))
        with DefaultStage(owner,family,index) as stage:
            original=[bytes(step) for step in stage.program.steps[:stage.program.step_count]]
            assert stage.on_budget_exhaustion in (capi.TACTIX_SEQUENCE_BUDGET_STOP,
                capi.TACTIX_SEQUENCE_BUDGET_CONTINUE,capi.TACTIX_SEQUENCE_BUDGET_FINISH_ROUND)
            ids={step.base.step_id for step in stage.program.steps[:stage.program.step_count]}
            assert set(stage.progress_followups)<=ids and set(stage.before_progress)<=ids
            assert isinstance(stage.structured_handoff_after_prior_progress,bool)
            steps=stage.steps_after_feedback(flags&1,flags&2,flags&4)
            actual=[]
            for step in steps:
                text=str(step.base.step_id)
                p=step.base.parameters
                if (p.kind==capi.TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY and
                        p.value.substitution_redundancy.producer==capi.TACTIX_ONLINE_SR_GRAPH_SYMMETRY):
                    sr=p.value.substitution_redundancy
                    text+=f',{sr.symmetry_nonforcing_cut_admission},{sr.symmetry_use_structured_generators_mode},{sr.symmetry_stabilize_structured_variables_mode}'
                actual.append(text)
            assert actual==expected,(row,actual)
            assert [bytes(step) for step in stage.program.steps[:stage.program.step_count]]==original
    # Escaping records retain all nested storage, even from a temporary owner.
    escaped=DefaultStage(owner,0,0).program
    escaped_steps=DefaultStage(owner,0,0).steps_after_feedback()
    gc.collect()
    assert escaped.steps[0].base.instances[0]==8
    assert escaped_steps[0].base.instances[0]==8
    temporary=DefaultStage(owner,0,0)
    storage=weakref.ref(temporary._storage)
    leased=temporary.program
    nested=leased.steps[0].base.instances
    temporary.close();del temporary,leased;gc.collect()
    assert storage() is not None and nested[0]==8
    del nested;gc.collect();assert storage() is None
    temporary=DefaultStage(owner,0,0)
    chosen_storage=weakref.ref(temporary._storage)
    chosen=(capi.tactix_online_step_v2*1)(temporary.steps_after_feedback()[0])
    temporary.close();del temporary
    temporary=DefaultStage(owner,0,0)
    repacked_storage=weakref.ref(temporary._storage)
    repacked=capi.tactix_online_program_v2(temporary.program.steps,1,capi.TACTIX_ONLINE_REPEAT_ONCE,1)
    temporary.close();del temporary;gc.collect()
    assert chosen_storage() is not None and repacked_storage() is not None
    assert chosen[0].base.instances[0]==repacked.steps[0].base.instances[0]==8
    with Solver(library,default_frontend=True) as target:
        executor=target.c.tactix_online_executor_create()
        assert executor
        try:
            for program in (escaped,repacked,
                    capi.tactix_online_program_v2(escaped_steps,len(escaped_steps),capi.TACTIX_ONLINE_REPEAT_ONCE,1),
                    capi.tactix_online_program_v2(chosen,len(chosen),capi.TACTIX_ONLINE_REPEAT_ONCE,1)):
                assert not target.c.tactix_online_executor_configure(executor,2,C.byref(program),1,capi.TACTIX_ONLINE_RECONFIGURE_RESET)
        finally:
            target._lib.dll.tactix_online_executor_release(executor)
    del program,chosen,repacked;gc.collect()
    assert chosen_storage() is None and repacked_storage() is None
    survivor=DefaultStage(owner,0,0)
    saved=survivor.program.steps[0].base.step_id
    assert not owner.c.tactix_stat(capi.TACTIX_STAT_INPUT_CLOSED)
    assert not owner.c.tactix_stat(capi.TACTIX_STAT_HEURISTICS_ATTACHED)
assert survivor.program.steps[0].base.step_id==saved
survivor.close();survivor.close()
try:survivor.program
except RuntimeError:pass
else:raise AssertionError('released stage snapshot accepted')
def pose(s,variant):
    for name,value in ([('TACTIX_FORCE_MODE','stable'),('TACTIX_BUMP_REASONS_RATE','0.12345678901234566'),('TACTIX_SLOT_PERMILLE','BVE:7'),('TACTIX_FACTOR_INIT_TICKS','777')] if variant else [])+([('TACTIX_FORCE_MODE','focused')] if variant==2 else []):
        assert not s.c.tactix_set_default_option(name.encode(),value.encode())
for variant in range(3):
    with Solver(library,default_frontend=True) as s:
        pose(s,variant)
        definition=s.default_definition
        assert definition.text==ordinary[variant]
        assert not s.c.tactix_stat(capi.TACTIX_STAT_INPUT_CLOSED)
        assert not s.c.tactix_stat(capi.TACTIX_STAT_HEURISTICS_ATTACHED)
        schedule=NativeInprocessSchedule.from_execution(s)
        for key,value in native_config.items():
            expected=777 if variant and key=='factor_initial_ticks' else value
            assert schedule.config[key]==expected,(key,schedule.config[key],expected)
        assert isinstance(definition['restart.initial_interval'],int)
        assert isinstance(definition['branching.logarithm_offset'],float)
        if variant:assert definition['analysis.bump_reason_rate']==float('0.12345678901234566')
        assert any(k.startswith('inprocess.structured_root_stage') and isinstance(v,str) and 'step=' in v for k,v in definition.items())
        assert 'step=' in definition['affine_model.parameters']
        s.add([1,2]);s.add([-1,2]);assert s.default_definition.text==definition.text
        assert not s.c.tactix_stat(capi.TACTIX_STAT_INPUT_CLOSED)
        assert s.default_solve()==10 and 2 in s.model
        # Previously returned Python snapshots survive later reader calls/close.
    assert definition.text==ordinary[variant]
    query_stages=[]
    def strategy(e,restored):
        pose(e,variant)
        assert e.default_definition.text==query[variant]
        with DefaultStage(e,capi.TACTIX_DEFAULT_STAGE_REGULAR,1) as stage:
            bve=[step.base.parameters.value.bve for step in stage.program.steps[:stage.program.step_count]
                 if step.base.parameters.kind==capi.TACTIX_ONLINE_PARAMETERS_BVE]
            assert bve and all(p.candidate_scope==capi.TACTIX_ONLINE_BVE_CANDIDATES_QUERY_PURE_CLOSURE for p in bve)
        query_stages.append(DefaultStage(e,0,0))
        return ScriptedCDCL()(e,restored)
    with Session(library,strategy=strategy) as session:
        session.add([1,2]);session.add([-1,2]);result=session.solve()
        assert result.status==10 and 2 in result.model
    assert query_stages[0].program.step_count
    query_stages[0].close()
# Real persistent native handoffs retain the two legacy setting seeds after
# an option is removed; a fresh-definition object must describe that policy.
rounds=[]
def native_history(e,restored):
    for name,value in [('TACTIX_CHRONO_LEVELS','123'),('TACTIX_RANDEC','0')]:
        assert not e.c.tactix_set_default_option(name.encode(),value.encode() if not rounds else None)
    before=e.default_definition
    assert before['analysis.chronological_levels']==123 and not before['branching.random_sequences']
    answer=e.default_solve()
    assert e.default_definition.text==before.text
    rounds.append(answer);return answer
with Session(library,strategy=native_history) as session:
    session.add([1,2]);session.add([-1,2])
    for _ in range(3):assert session.solve().status==10
with Solver(library,default_frontend=True) as s:
    for name,value in [('TACTIX_ORDER_BVE','invalid'),('TACTIX_SLOT_PERMILLE','BVE:1001'),('TACTIX_CURSOR_VIVIFY','1'),('TACTIX_ORDER_FACTOR','native')]:
        assert not s.c.tactix_set_default_option(name.encode(),value.encode())
        try:s.default_definition
        except ValueError:pass
        else:raise AssertionError(name)
        assert not s.c.tactix_stat(capi.TACTIX_STAT_INPUT_CLOSED)
        assert not s.c.tactix_set_default_option(name.encode(),None)
    s.add([1,2]);assert s.default_solve()==10
# Preserve nested dotted metadata in braced programs without treating it as
# an outer config field; preserve full-width integers and exact double values.
sample=StrategyDefinition('strategy-v1 a.large=18446744073709551615 a.program={0:sequence-repeat=once:1 step=1:X:note.inner=7} a.rate=0.12345678901234566 a.exponent=1.25e-12 a.negative_zero=-0.0')
assert len(sample)==5 and sample['a.large']==2**64-1
assert 'note.inner=7' in sample['a.program'] and sample['a.rate']==float('0.12345678901234566')
import math
assert sample['a.exponent']==1.25e-12 and math.copysign(1,sample['a.negative_zero'])==-1
for bad in ['strategy-v2 x.y=1','strategy-v1 x.y={oops','strategy-v1 x.y=1 x.y=2']:
    try:StrategyDefinition(bad)
    except ValueError:pass
    else:raise AssertionError(bad)
print('Native configured definitions: ordinary/query/options, pure input/H reads, exact scalar/program snapshots, installed Python phase configuration; full controller remains open')
