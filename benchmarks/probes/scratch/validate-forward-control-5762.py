#!/usr/bin/env python3
"""Compile actual-code fixed witnesses and a trace-only exit observer."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5760-forward-exit-observer'
source = root / '.claude/worktrees/runtime-forward-exit-observer-5760'
commands = []
def run(label,command):
    command=list(map(str,command));commands.append(command)
    r=subprocess.run(command,cwd=root,capture_output=True,text=True)
    p=c / 'logs' / (label+('' if len(sys.argv)==1 else '-'+sys.argv[1])+'.log')
    assert not p.exists()
    p.write_text(r.stdout+r.stderr)
    if r.returncode: raise RuntimeError((label,r.returncode,r.stdout[-2000:],r.stderr[-2000:]))
    return r.stdout
parent = root / 'build/pin-5741-reuse-target-release'
link=shlex.split((parent / 'tests/CMakeFiles/test_strengthen.dir/link.txt').read_text())
objects=[(parent / 'tests' / name).resolve() for name in link if name.endswith('.o')][1:]
fixture=root / 'benchmarks/probes/scratch/forward-unit-control-5761.cpp'
native_fixture=root / 'benchmarks/probes/scratch/forward-unit-native-5761.c'
native=root / 'tools/solvers/variants/kissat-score-bump-observer-5696'
run('compile-control',[link[0],'-O3','-std=c++20','-fno-exceptions','-fno-rtti',
    '-I'+str(source / 'src'),'-I'+str(source / 'include'),fixture,*objects,'-o',c / 'control'])
control=run('control',[c / 'control'])
run('compile-native',['gcc','-O3','-DNDEBUG','-I'+str(native / 'src'),native_fixture,native / 'build/libkissat.a','-lm','-o',c / 'native'])
reference=run('native',[c / 'native'])
assert control.count('CONTROL tail=') == reference.count('NATIVE tail=') == 2
print(control+reference,flush=True)
b=root / 'build/pin-5760-forward-exit-trace'
run('configure',['cmake','-S',source,'-B',b,'-DCMAKE_BUILD_TYPE=Release','-DTACTIX_ENABLE_EVENT_TRACE=ON'])
run('build',['cmake','--build',b,'-j8'])
run('ctest',['ctest','--test-dir',b,'--output-on-failure','-j8'])
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
(c / 'fixed-control.json').write_text(json.dumps(dict(commands=commands,
    hashes={str(p.relative_to(root)):sha(p) for p in [fixture,native_fixture,native / 'src/forward.c',native / 'build/libkissat.a',*objects,c / 'control',c / 'native',b / 'tactix']},
    control=control,native=reference,
    conclusion='Actual Tactix stops after a root-producing candidate,leaving independent subsumable tail;actual native forward pass derives same unit and subsumes the tail. Existing unit-named test only exercises entry propagation,not this branch.',
    limitation='Two fixed states,not proof/model fuzz or evidence this causes Rank31. Trace-only observer;no solver repair yet.'),indent=2)+'\n')
print('PASS actual-code unit/tail controls and trace suite',flush=True)
