from pathlib import Path
import json, subprocess, sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6582-material-proof-control'
source=root/'.claude/worktrees/runtime-material-schedule-6579'
pin=root/'build/pin-6579-material-schedule-release'
fixture=Path(__file__).with_name('material-proof-diagnostic-6583.cpp')
binary=c/'diagnostic-6583'
assert not binary.exists()
command=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),*map(str,sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))),'-o',str(binary)]
subprocess.run(command,check=True)
args=[str(binary),'0','3','0','1',str(c/'diagnostic.cnf'),str(c/'diagnostic.proof'),str(c/'diagnostic.effects.json')]
p=subprocess.run(args,capture_output=True,text=True)
with (c/'diagnostic-6583.json').open('x') as out:
    json.dump(dict(command=args,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr,
        binary_sha256=sha(binary),source_sha256=sha(fixture),initial_binary_sha256=sha(c/'fixture')) ,out,indent=2)
assert p.returncode==-6
print('\n'.join(p.stderr.splitlines()[:5]+p.stderr.splitlines()[-3:]))
