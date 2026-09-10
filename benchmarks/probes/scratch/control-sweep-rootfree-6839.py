#!/usr/bin/env python3
"""Actual old/new Sweep environment and existing full-family fixed controls."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6838-sweep-rootfree-build'
target = c/'control-6839.json'
assert not target.exists()
source = root/'.claude/worktrees/runtime-sweep-rootfree-6838'
parent = root/'.claude/worktrees/runtime-reduce-watermark-6730'
pin = root/'build/pin-6730-reduce-watermark-release'
objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
other = [p for p in objects if not str(p).endswith('/rules/sweep/sweep.cpp.o')]
assert len(other) == len(objects) - 1
hashes = {str(p.relative_to(root)):sha(p) for p in objects}
outputs = {}
def run(name, command):
    r = subprocess.run(command, cwd=c, text=True, capture_output=True)
    log = c/'logs'/f'{name}.json'
    with log.open('x') as stream:
        json.dump(dict(command=command, exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr), stream, indent=2); stream.write('\n')
    hashes[str(log.relative_to(root))] = sha(log)
    assert r.returncode == 0, (name, r.returncode, r.stderr[-2000:])
    return r.stdout
for arm, tree in (('previous', parent), ('candidate', source)):
    flags = ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O3', '-DNDEBUG', '-fno-exceptions', '-fno-rtti', '-I'+str(tree/'src'), '-I'+str(tree/'include')]
    obj = c/f'{arm}-sweep.o'; assert not obj.exists()
    cpp = tree/'src/rules/sweep/sweep.cpp'; hashes[str(cpp.relative_to(root))] = sha(cpp)
    run(arm+'-compile', flags+['-c', str(cpp), '-o', str(obj)])
    outputs[arm] = {}
    for name in ('test_sweep_rootfree', 'test_sweep'):
        fixture = source/'tests'/f'{name}.cpp'; hashes[str(fixture.relative_to(root))] = sha(fixture)
        exe = c/f'{arm}-{name}'; assert not exe.exists()
        run(arm+'-'+name+'-link', flags+[str(fixture), str(obj), *map(str, other), '-o', str(exe)])
        outputs[arm][name] = run(arm+'-'+name+'-run', [str(exe)])
assert outputs['previous'] == outputs['candidate'], outputs
hashes[str(Path(__file__).relative_to(root))] = sha(Path(__file__))
with target.open('x') as stream:
    json.dump(dict(hashes=hashes, outputs=outputs,
        conclusion='Actual old/new environments, order, proof identities, stats, model sets and builder proof bytes match300 fixed cases. Existing Sweep family tests pass identically. Scratch contents intentionally not semantic state. Independent final proof/B/C/full-runtime and current-source fuzz remain separate.'), stream, indent=2); stream.write('\n')
print(json.dumps(outputs, indent=2))
