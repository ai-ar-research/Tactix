#!/usr/bin/env python3
"""Compile three fixed API fixtures against the immutable5900 object pin."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
out=root/'benchmarks/runs/20260908-5952-bve-retired-reentry-controls'
out.mkdir(exist_ok=False)
build=root/'build/pin-5900-reduce-root-release'
source=root/'.claude/worktrees/runtime-reduce-root-5900'
fixture=Path(__file__).with_name('probe-bve-retired-reentry-5952.cpp')
objects=sorted((build/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
cmd=['g++','-std=c++20','-O2','-fno-exceptions','-fno-rtti','-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),*map(str,objects),'-o',str(out/'probe')]
r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,(r.stdout,r.stderr)
r=subprocess.run([out/'probe'],capture_output=True,text=True)
(out/'output.txt').write_text(r.stdout+r.stderr)
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
(out/'summary.json').write_text(json.dumps(dict(command=cmd,output=r.stdout,exit_code=r.returncode,
    binary_sha256=hashlib.sha256((out/'probe').read_bytes()).hexdigest(),
    fixture_sha256=hashlib.sha256(fixture.read_bytes()).hexdigest(),
    scope='Three fixed kernel API fixtures, no generated fuzz or real-instance solve; no performance claim.'),indent=2)+'\n')
print(r.stdout)
