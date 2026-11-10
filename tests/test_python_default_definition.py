"""Native solve-entry configuration versus public C/Python definition reads."""
from pathlib import Path
import ctypes as C
import os
import subprocess
import sys
library, fixture, observations, root = map(Path, sys.argv[1:])
sys.path[:0] = [str(root/'python'), str(root/'tests')]
from tactix import Solver, Session, ScriptedCDCL, capi
from tactix.definition import StrategyDefinition
from python_policy_reference import NativeInprocessSchedule
import json
native_config=json.loads(subprocess.check_output([str(observations)],text=True).splitlines()[0])['config']
# Session-local settings must override the environment, just as native solve does.
os.environ['TACTIX_FORCE_MODE']='focused'
ordinary=subprocess.check_output([str(fixture)],text=True).splitlines()
query=subprocess.check_output([str(fixture),'--query'],text=True).splitlines()
assert len(ordinary)==len(query)==3 and ordinary[0]!=query[0]
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
    def strategy(e,restored):
        pose(e,variant)
        assert e.default_definition.text==query[variant]
        return ScriptedCDCL()(e,restored)
    with Session(library,strategy=strategy) as session:
        session.add([1,2]);session.add([-1,2]);result=session.solve()
        assert result.status==10 and 2 in result.model
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
