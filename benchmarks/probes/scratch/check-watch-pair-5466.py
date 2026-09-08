#!/usr/bin/env python3
"""Compare a fixed header-only fixture on actual old and new watch methods."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5466-watch-pair'
fixture=root/'.claude/worktrees/runtime-watch-pair-5466/tests/test_watch_pair.cpp'
outputs=[]
for arm,source in [('old','runtime-bound-advance-5459'),('new','runtime-watch-pair-5466')]:
    binary=root/'benchmarks/probes/scratch'/f'watch-pair-{arm}-5466'
    assert not binary.exists()
    subprocess.run(['g++','-std=c++20','-O3','-I'+str(root/'.claude/worktrees'/source/'src'),
                    str(fixture),'-o',str(binary)],check=True)
    result=subprocess.run([binary],capture_output=True,text=True)
    log=c/'logs'/f'fixture-{arm}.log';assert not log.exists()
    log.write_text(result.stdout+result.stderr+f'exit={result.returncode}\n')
    print(arm,result.stdout+result.stderr,flush=True)
    assert result.returncode==0
    outputs.append(result.stdout)
assert outputs[0]==outputs[1]
print('PASS: actual old/new complete physical-state digests identical')
