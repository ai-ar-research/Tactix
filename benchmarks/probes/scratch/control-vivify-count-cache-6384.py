#!/usr/bin/env python3
"""Actual old/new scheduler, transactions, proof bytes and ABI-size controls."""
import json
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260909-6383-vivify-count-cache-build'
target = c/'control-6384.json'; assert not target.exists()
source = root/'.claude/worktrees/runtime-vivify-count-cache-6383'
parent = root/'.claude/worktrees/runtime-compact-stamp-6357'
pin = root/'build/pin-6357-compact-stamp-release'
objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert objects
compiler = '/cm/local/apps/gcc/13.1.0/bin/c++'
records = {}; layouts = {}; outputs = {}
def run(name, command):
    p = subprocess.run(command, cwd=c, capture_output=True, text=True)
    log = c/'logs'/f'{name}.json'; assert not log.exists()
    log.write_text(json.dumps(dict(command=command, exit_code=p.returncode,
        stdout=p.stdout, stderr=p.stderr), indent=2)+'\n')
    records[str(log.relative_to(root))] = sha(log)
    assert p.returncode == 0, (name, p.returncode, p.stderr[-2000:])
    return p.stdout
for arm, tree in (('previous', parent), ('candidate', source)):
    obj = c/f'{arm}-vivify.o'; dump = c/f'{arm}-vivify.class'
    assert not obj.exists() and not dump.exists()
    flags = [compiler, '-std=c++20', '-O3', '-DNDEBUG', '-fno-exceptions', '-fno-rtti',
        '-I'+str(tree/'src'), '-I'+str(tree/'include')]
    run(arm+'-compile', flags+['-fdump-lang-class='+str(dump), '-c',
        str(tree/'src/rules/vivify/vivify.cpp'), '-o', str(obj)])
    match = re.search(r'Class tactix::VivifyCand\n\s+size=(\d+) align=(\d+)', dump.read_text())
    assert match
    layouts[arm] = dict(size=int(match[1]), alignment=int(match[2]),
        dump_sha256=sha(dump), source_sha256=sha(tree/'src/rules/vivify/vivify.cpp'))
    outputs[arm] = {}
    other = [p for p in objects if not str(p).endswith('/rules/vivify/vivify.cpp.o')]
    assert len(other) == len(objects)-1
    for name in ('test_vivify_count_cache', 'test_vivify_max'):
        exe = c/f'{arm}-{name}'; assert not exe.exists()
        fixture = source/'tests'/f'{name}.cpp'
        run(arm+'-'+name+'-link', flags+[str(fixture), str(obj), *map(str, other), '-o', str(exe)])
        outputs[arm][name] = run(arm+'-'+name+'-run', [str(exe)])
assert layouts['previous']['size'] == layouts['candidate']['size'] == 40
assert layouts['previous']['alignment'] == layouts['candidate']['alignment'] == 8
assert outputs['previous'] == outputs['candidate'], outputs
target.write_text(json.dumps(dict(layouts=layouts, outputs=outputs, logs=records,
    fixtures={str(p.relative_to(root)):sha(p) for p in (source/'tests/test_vivify_count_cache.cpp', source/'tests/test_vivify_max.cpp')},
    parent_objects={str(p.relative_to(root)):sha(p) for p in objects},
    conclusion='Exact actual old/new query ordering, repeated canonical state digest and proof-byte digest; exhaustive 8-variable model sets preserved after each dose. Layout unchanged40B/align8. Not independent final proof acceptance or a runtime claim.'), indent=2)+'\n')
print(json.dumps(dict(layouts=layouts, outputs=outputs), indent=2))
