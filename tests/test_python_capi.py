"""Check the complete Python declarations against the C compiler and live library."""
from pathlib import Path
import ctypes as C
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
finally:lib.tactix_release(handle)

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
print(f'Python C ABI: {len(s.FUNCTIONS)} symbols/signatures, {len(s.PUBLIC_TYPES)} types, '
      f'{len(s.RECORDS)} record layouts, {len(s.CONSTANTS)} constants; typed calls and guarded views pass')
