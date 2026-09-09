#!/usr/bin/env python3
"""Compile the fixed regression against immutable5962 and confirm old behavior."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
out=root/'benchmarks/runs/20260908-6049-reduce-root-lock/old-controls'; out.mkdir(exist_ok=False)
source=root/'.claude/worktrees/runtime-bve-active-pop-5962'
build=root/'build/pin-5962-bve-active-pop-release'
fixture=root/'.claude/worktrees/runtime-reduce-root-lock-6049/tests/test_reduce_root_lock.cpp'
cmd=['g++','-std=c++20','-O2','-fno-exceptions','-fno-rtti','-DTACTIX_EXPECT_OLD_REDUCE_ROOT_LOCK',
     '-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),
     *map(str,sorted((build/'CMakeFiles/tactix_lib.dir').rglob('*.o'))),'-o',str(out/'old')]
for name,command in [('compile',cmd),('run',[str(out/'old')])]:
    r=subprocess.run(command,cwd=root,capture_output=True,text=True)
    (out/(name+'.log')).write_text(r.stdout+r.stderr)
    print(name,r.returncode,r.stdout+r.stderr,flush=True)
    assert r.returncode==0
