"""Check the complete Python declarations against the C compiler and live library."""
from pathlib import Path
import ctypes as C
import gc
import hashlib
import importlib.util
import subprocess
import sys
import tempfile

library, root, compiler = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
sys.path.insert(0,str(root/'python'))
from tactix import capi, Session, ScriptedCDCL, SAT
s=capi.schema
for name,digest in s.HEADER_SHA256.items():
    assert hashlib.sha256((root/'include'/name).read_bytes()).hexdigest()==digest, 'regenerate Python declarations: '+name
assert all(hasattr(capi,name) for name in s.PUBLIC_TYPES)
for name in s.RECORDS:
    record = capi._TYPES[name]
    try: record(__tactix_unknown_field__=1)
    except TypeError: pass
    else: raise AssertionError('unknown field silently ignored: ' + name)
try: capi.tactix_online_parameters(tag=capi.TACTIX_ONLINE_PARAMETERS_SUBSTITUTE)
except TypeError: pass
else: raise AssertionError('misspelled discriminator silently selected INHERIT')
parameters = capi.tactix_online_parameters(kind=capi.TACTIX_ONLINE_PARAMETERS_SUBSTITUTE)
assert parameters.kind == capi.TACTIX_ONLINE_PARAMETERS_SUBSTITUTE
lib=capi.load_library(library)
assert set(capi.SIGNATURES)==set(s.FUNCTIONS)

# The compiler evaluates the actual public definitions, including the nested
# union, arrays and all field offsets. Header hashes prohibit stale coverage.
def c_declaration(code,name):
    if isinstance(code,str):
        return ('struct tactix_solver' if code=='tactix_solver' else code)+' '+name
    kind,value,*extra=code
    if kind=='*':return c_declaration(value,'*'+name)
    if kind=='function':
        params=', '.join(c_declaration(t,'').strip() for t in extra[0]) or 'void'
        return c_declaration(value,'(*'+name+')('+params+')')
    if kind=='array':return c_declaration(value,name+'['+str(extra[0])+']')
    raise AssertionError(code)

# Const qualifiers do not affect the calling ABI. Erase them only in this
# declaration-only probe so signatures compare directly with ctypes types.
source=['#include <stddef.h>','#include <stdint.h>','#include <stdio.h>',
        '#define const','#include "tactix.h"','#include "tactix_ipasir.h"','#undef const']
for name,(result,params) in s.FUNCTIONS.items():
    expected_type='expected_'+name
    source.append('typedef '+c_declaration(('function',result,params),expected_type)+';')
    source.append(f'_Static_assert(__builtin_types_compatible_p(__typeof__(&{name}), {expected_type}), "{name} signature");')
source.append('int main(void) {')
expected=[]
for name,(_,ctype,fields) in s.RECORDS.items():
    t=capi._TYPES[name]
    source.append(f'printf("%zu %zu", sizeof({ctype}), _Alignof({ctype}));')
    row=[C.sizeof(t), C.alignment(t)]
    for field,_ in fields:
        source.append(f'printf(" %zu", offsetof({ctype}, {field}));')
        row.append(getattr(t,field).offset)
    source.append('puts("");');expected.append((name,row))
for name,value in s.CONSTANTS.items():
    source.append(f'printf("%llu\\n", (unsigned long long)({name}));')
    expected.append((name,[value & ((1<<64)-1)]))
for value in (-2147483648,-1,0,1,2147483647):
    source.append(f'printf("%llu\\n", (unsigned long long)TACTIX_LIT_INSTANCE({value}));')
    expected.append(('literal-instance',[capi.TACTIX_LIT_INSTANCE(value)]))
source.append('return 0;}')
with tempfile.TemporaryDirectory(prefix='tactix-python-abi-') as tmp:
    p=Path(tmp);(p/'probe.c').write_text('\n'.join(source))
    subprocess.run([compiler,'-std=gnu11','-I'+str(root/'include'),str(p/'probe.c'),'-o',str(p/'probe')],check=True)
    rows=subprocess.check_output([str(p/'probe')],text=True).splitlines()
    assert len(rows)==len(expected)
    for line,(name,values) in zip(rows,expected):assert list(map(int,line.split()))==values,(name,line,values)

# Typed parameter readback, discovery, and all program versions go through the
# same native ABI; invalid programs remain native errors rather than crashes.
handle=lib.tactix_init();assert handle
executor=lib.tactix_online_executor_create(handle);assert executor
try:
    settings=capi.tactix_work_settings(fields=capi.TACTIX_WORK_BUDGET|capi.TACTIX_WORK_OFFSET,
                                     budget=123456,offset=17)
    assert lib.tactix_install_work_settings(handle,C.byref(settings))==0
    actual=capi.tactix_work_settings();assert lib.tactix_read_work_settings(handle,C.byref(actual))==0
    assert (actual.budget,actual.offset)==(123456,17)
    for version in range(1,18):
        suffix='' if version==1 else '_v'+str(version)
        program=getattr(capi,'tactix_online_program'+suffix)()
        # A negative step count is invalid in every public version.
        program.step_count=-1
        assert getattr(lib,'tactix_online_configure'+suffix)(handle,C.byref(program),version,0)==-1
        assert lib.tactix_online_executor_configure(handle,executor,version,
            C.byref(program),version,0)==-1
finally:
    lib.tactix_online_executor_release(executor)
    lib.tactix_release(handle)

# Positive nested payloads through every compatible program version. The C
# online API fixture uses this same Condition tuple: it removes the duplicated
# clause, then reconstruction must repair decisions falsifying that clause.
# Only configure owns the storage after this helper returns.
def configure_condition(handle, version, executor=None):
    clause=(C.c_int*2)(1,2)
    witness=(C.c_int*3)(1,3,4)
    cubes=(capi.tactix_online_condition_instance*2)(
        capi.tactix_online_condition_instance(clause,2,witness,3),
        capi.tactix_online_condition_instance(clause,2,witness,3))
    step=getattr(capi,'tactix_online_step_v'+str(version))()
    payload=step
    while not hasattr(payload,'condition_instances'):payload=payload.base
    payload.condition_instances=cubes;payload.condition_instance_count=2
    base=step
    while hasattr(base,'base'):base=base.base
    base.step_id=375
    base.rule_id=next(i for i in range(lib.tactix_rule_count())
                      if lib.tactix_rule_name(i)==b'Condition')
    base.granularity=capi.TACTIX_ONLINE_GRANULARITY_BATCH
    base.repeat_mode=capi.TACTIX_ONLINE_REPEAT_ONCE;base.repeat_limit=1
    base.budget_unlimited=1
    program=getattr(capi,'tactix_online_program_v'+str(version))(
        C.pointer(step),1,capi.TACTIX_ONLINE_REPEAT_ONCE,1)
    if executor:
        assert lib.tactix_online_executor_configure(handle,executor,version,
            C.byref(program),2,capi.TACTIX_ONLINE_RECONFIGURE_RESET)==0
    else:
        assert getattr(lib,'tactix_online_configure_v'+str(version))(
            handle,C.byref(program),2,capi.TACTIX_ONLINE_RECONFIGURE_RESET)==0
    clause[:]=(0,0);witness[:]=(0,0,0)

formula=((1,2),(1,2),(-1,3),(-3,4))
for version in range(3,18):
    handle=lib.tactix_init();assert handle
    try:
        for clause in formula:
            for literal in (*clause,0):assert lib.tactix_add_checked(handle,literal)==0
        configure_condition(handle,version);gc.collect()
        result=capi.tactix_online_result()
        assert lib.tactix_online_advance(handle,C.byref(result))==0
        assert (result.status,result.made_rule_progress,result.applications,
                result.instance,result.configuration_revision)==(
                    capi.TACTIX_ONLINE_COMPLETED,1,1,0,2),(version,result.status)
        assert lib.tactix_online_advance(handle,C.byref(result))==0
        assert (result.status,result.applications,result.work)==(
            capi.TACTIX_ONLINE_STALE_REQUEST,0,0),version
        for literal in (-1,-2):assert lib.tactix_decide(handle,literal)==0
        assert lib.tactix_solve(handle)==SAT
        model={lib.tactix_value(handle,v) for v in range(1,5)}
        assert all(any(l in model for l in clause) for clause in formula),(version,model)
    finally:lib.tactix_release(handle)

# Independent native continuations expose the same owned versioned payloads
# through Python. A cloned pending selection preserves its spent work and
# detects stale identities; handles remain safely releasable after the view.
for version in range(3,18):
    handle=lib.tactix_init();executor=lib.tactix_online_executor_create(handle)
    clone=None
    try:
        for clause in formula:
            for literal in (*clause,0):assert lib.tactix_add_checked(handle,literal)==0
        configure_condition(handle,version,executor);gc.collect()
        result=capi.tactix_online_execution_result()
        assert lib.tactix_online_executor_advance(handle,executor,None,C.byref(result))==0
        assert (result.base.status,result.base.applications,result.base.instance)==(
            capi.TACTIX_ONLINE_COMPLETED,1,0),version
        clone=lib.tactix_online_executor_clone(executor);assert clone
        for continuation in (executor,clone):
            assert lib.tactix_online_executor_consumed(continuation,375)==1
            assert lib.tactix_online_executor_advance(handle,continuation,None,C.byref(result))==0
            assert result.base.status==capi.TACTIX_ONLINE_STALE_REQUEST,version
        for literal in (-1,-2):assert lib.tactix_decide(handle,literal)==0
        assert lib.tactix_solve(handle)==SAT
        model={lib.tactix_value(handle,v) for v in range(1,5)}
        assert all(any(l in model for l in clause) for clause in formula),(version,model)
    finally:
        lib.tactix_release(handle)
        assert lib.tactix_online_executor_complete(executor)==-1
        lib.tactix_online_executor_release(executor)
        if clone:lib.tactix_online_executor_release(clone)

# Bound native calls preserve the Execution lifetime/re-entry guards.
views=[];policy=ScriptedCDCL()
def inspected(execution,restored):
    views.append(execution.c)
    assert execution.c.tactix_stat(capi.TACTIX_STAT_CONFLICTS)!=(1<<64)-1
    ids=(C.c_int*lib.tactix_rule_count())()
    assert execution.c.tactix_rules_applicable(ids,len(ids))>=0
    try:execution.c.tactix_release
    except AttributeError:pass
    else:raise AssertionError('borrowed view exposes release')
    return policy(execution,restored)
with Session(library=library,strategy=inspected) as session:
    session.add([1,2]);session.add([1,-2]);assert session.solve().status==SAT
try:views[-1].tactix_stat(capi.TACTIX_STAT_CONFLICTS)
except RuntimeError:pass
else:raise AssertionError('expired C view accepted')
# Shared restart observations are callable from an independently authored policy.
restart_policy=ScriptedCDCL()
def restart_planning(execution,restored):
    views.append(execution.c)
    while True:
        action=execution.next()
        if action==capi.TACTIX_QUERY_ASSUME: execution.assume_next()
        elif action==capi.TACTIX_QUERY_PROPAGATE: execution.apply('Propagate')
        else:
            assert action==capi.TACTIX_QUERY_READY
            break
    floor=C.c_uint32(99)
    assert execution.c.tactix_query_assumption_floor(C.byref(floor))==0 and floor.value==1
    ranked=tuple(map(abs,execution.candidates()))
    assert len(ranked)>=3
    next_literal=ranked[0]
    for literal in ranked[-2:]:
        execution.decide(literal)
        execution.apply('Propagate')
    level=execution.c.tactix_stat(capi.TACTIX_STAT_LEVEL)
    keep=C.c_uint32(99)
    assert execution.c.tactix_restart_reuse_level(-next_literal,floor.value,C.byref(keep))==0
    assert floor.value<=keep.value<level
    selected_keep=keep.value
    invalid=C.c_uint32(77)
    assert execution.c.tactix_restart_reuse_level(0,0,C.byref(invalid))==-1 and invalid.value==77
    assert execution.c.tactix_restart_reuse_level(next_literal,level,C.byref(keep))==0 and keep.value==level
    assert execution.apply('Restart',keep.value)==0
    assert execution.c.tactix_stat(capi.TACTIX_STAT_LEVEL)==level
    assert execution.c.tactix_query_assumption_floor(C.byref(floor))==0 and floor.value==1
    assert execution.apply('Restart',selected_keep)==0
    assert execution.c.tactix_stat(capi.TACTIX_STAT_LEVEL)==selected_keep
    assert execution.c.tactix_query_assumption_floor(C.byref(floor))==0 and floor.value==1
    assert execution.next()==capi.TACTIX_QUERY_READY
    return restart_policy(execution,restored)
with Session(library=library,strategy=restart_planning) as session:
    for clause in ([1,2],[3,4],[5,6],[7,8]): session.add(clause)
    assert session.solve([1]).status==SAT
try: views[-1].tactix_query_assumption_floor(C.byref(C.c_uint32()))
except RuntimeError: pass
else: raise AssertionError('restart observation accepted expired execution')
try: views[-1].tactix_restart_reuse_level(7,0,C.byref(C.c_uint32()))
except RuntimeError: pass
else: raise AssertionError('reuse observation accepted expired execution')
print(f'Python C ABI: {len(s.FUNCTIONS)} symbols/signatures, {len(s.PUBLIC_TYPES)} types, '
      f'{len(s.RECORDS)} record layouts, {len(s.CONSTANTS)} constants; '
      '30 owned nested programs, independent continuations, typed calls and guarded views pass')
