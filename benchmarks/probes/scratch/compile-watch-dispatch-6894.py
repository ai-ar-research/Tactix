#!/usr/bin/env python3
"""Inspect actual kernel object shape before paying for an experiment build."""
from pathlib import Path
import difflib
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6893-watch-dispatch-build'
source = root/'.claude/worktrees/runtime-watch-dispatch-6893'
parent = root/'.claude/worktrees/runtime-reduce-watermark-6730'
target = c/'compile-6894.json'; assert not target.exists()
objects = {}; symbols = {}
for arm,tree in (('previous',parent),('candidate',source)):
    obj = c/(arm+'-kernel.o');assert not obj.exists()
    command = ['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O3','-DNDEBUG','-fno-exceptions','-fno-rtti',
        '-I'+str(tree/'src'),'-I'+str(tree/'include'),'-c',str(tree/'src/kernel/kernel.cpp'),'-o',str(obj)]
    result = subprocess.run(command,capture_output=True,text=True)
    log = c/'logs'/(arm+'-compile-6894.json')
    with log.open('x') as stream:
        json.dump(dict(command=command,exit_code=result.returncode,stdout=result.stdout,stderr=result.stderr),stream,indent=2);stream.write('\n')
    assert result.returncode == 0,(arm,result.stderr[-2000:])
    objects[arm] = dict(path=str(obj.relative_to(root)),sha256=sha(obj))
    rows = subprocess.check_output(['nm','-S','--defined-only',str(obj)],text=True).splitlines()
    symbols[arm] = {fields[-1]:int(fields[1],16) for row in rows if len(fields:=row.split()) == 4
        and ('propagate_literal' in fields[-1] or 'kernel_propagate' in fields[-1])}
    print(arm,symbols[arm],flush=True)
before = (parent/'src/kernel/kernel.cpp').read_text()
after = (source/'src/kernel/kernel.cpp').read_text()
patch = ''.join(difflib.unified_diff(before.splitlines(True),after.splitlines(True),fromfile='a/src/kernel/kernel.cpp',tofile='b/src/kernel/kernel.cpp'))
with (c/'compile-scope.patch').open('x') as stream:stream.write(patch)
paths = [Path(__file__),source/'src/kernel/kernel.cpp',parent/'src/kernel/kernel.cpp',c/'compile-scope.patch',c/'parent.json']
with target.open('x') as stream:
    json.dump(dict(objects=objects,symbols=symbols,hashes={str(p.relative_to(root)):sha(p) for p in paths},
        limitation='Compile-only shape check, no runtime or correctness acceptance. Same-layout source change restricted to full watch consumption and one shared blocker test; no budget, scheduling, reason or layout change intended. Actual old/new fixed watch-state/counter controls and full suites/proofs/B/C required before timing.'),stream,indent=2);stream.write('\n')
