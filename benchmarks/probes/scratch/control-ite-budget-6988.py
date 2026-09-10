#!/usr/bin/env python3
"""Private ITE cuts against both actual complete Congruence translation units."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build'
target = c/'budget-control-6988.json'; assert not target.exists()
control = c/'control-6985.json'; m = json.loads(control.read_text())
for rel, digest in m['hashes'].items(): assert sha(root/rel) == digest, rel
fixture = root/'benchmarks/probes/scratch/test-ite-budget-6987.cpp'
objects = [root/p for p in m['hashes'] if '/CMakeFiles/tactix_lib.dir/' in p and p.endswith('.o') and not p.endswith('/congruence/congruence.cpp.o')]
assert objects
paths = [Path(__file__), fixture, control]; outputs = {}
for arm, name in (('previous','reduce-watermark-6730'), ('candidate','ite-pair-hoist-6969')):
    tree = root/('.claude/worktrees/runtime-'+name)
    exe = c/(arm+'-budget-6988'); assert not exe.exists()
    command = ['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O3','-DNDEBUG',
        '-fno-exceptions','-fno-rtti','-I'+str(tree/'src'),'-I'+str(tree/'include'),
        '-include',str(tree/'src/rules/congruence/congruence.cpp'),str(fixture),*map(str,objects),'-o',str(exe)]
    for action, cmd in (('build',command),('run',[str(exe)])):
        log = c/'logs'/f'{arm}-budget-{action}-6988.json'; assert not log.exists()
        p = subprocess.run(cmd, capture_output=True, text=True)
        with log.open('x') as out:
            json.dump(dict(command=cmd, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr), out, indent=2); out.write('\n')
        paths.append(log)
        assert p.returncode == 0, (arm, action, p.returncode, p.stderr[-2000:])
        if action == 'run': outputs[arm] = p.stdout
    paths.append(exe)
assert outputs['previous'] == outputs['candidate'], outputs
with target.open('x') as out:
    json.dump(dict(outputs=outputs, hashes={str(p.relative_to(root)):sha(p) for p in paths},
                   scope='448 actual ITE meter/termination/unfinished-index cases, both orders, collection/pairing effort boundaries, exactly256/over256 conditional lists and same-index resumption. Exact full gate payload/tick/interruption/cut digest. No new solver semantics or public API.'), out, indent=2); out.write('\n')
print(outputs)
