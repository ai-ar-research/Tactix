#!/usr/bin/env python3
"""Actual immutable parent negative test; preserve complete commands/results."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260911-7364-lucky-root-parent'
assert not c.exists();c.mkdir()
source=root/'.claude/worktrees/runtime-debug-contracts-7339'
test=Path(__file__).with_name('lucky-root-consumer-7364.cpp')
objects=sorted((root/'build/pin-7340-debug-contracts-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert objects
exe=c/'test_lucky_root_consumer'
hashes={str(p.relative_to(root)):sha(p) for p in [Path(__file__),test,*objects]}
commands=[('build',['/cm/local/apps/gcc/13.1.0/bin/g++','-std=c++20','-O3','-DNDEBUG',
    '-fno-exceptions','-fno-rtti','-I'+str(source/'src'),'-I'+str(source/'include'),test,*objects,'-o',exe],0),
    ('parent-characterization',[exe,'--expect-missing'],0),
    ('parent-regression',[exe],-6)]
for name,command,expected in commands:
    command=list(map(str,command));r=subprocess.run(command,cwd=c,text=True,capture_output=True)
    path=c/(name+'.json')
    with path.open('x') as out:json.dump(dict(command=command,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr),out,indent=2)
    hashes[str(path.relative_to(root))]=sha(path)
    print(name,r.returncode,r.stdout,r.stderr,flush=True)
    assert r.returncode==expected
hashes[str(exe.relative_to(root))]=sha(exe)
with (c/'manifest.json').open('x') as out:json.dump(dict(status='PASS_NEGATIVE_CONTROL',hashes=hashes,
    decision='Actual DefaultPolicy early Lucky with supplied H leaves proved roots queued. Root counts1/5/29, both import directions and physical-fold modes reproduce; desired assertion fails before repair.'),out,indent=2)
