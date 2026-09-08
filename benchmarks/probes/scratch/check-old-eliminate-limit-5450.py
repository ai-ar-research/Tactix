#!/usr/bin/env python3
"""Demonstrate the extra round with immutable predecessor scheduler objects."""
from pathlib import Path
import shlex
import subprocess
root=Path(__file__).resolve().parents[3]
build=root/'build/pin-5445-replacement-cursor-release'
command=shlex.split((build/'tests/CMakeFiles/test_slot_pose.dir/link.txt').read_text())
objects=[str((build/'tests'/arg).resolve()) for arg in command
    if arg.endswith('.o') and 'test_slot_pose.cpp.o' not in arg]
target=root/'benchmarks/probes/scratch/eliminate-limit-old-5450'
assert not target.exists()
subprocess.run([command[0],'-std=c++20','-O3','-DTACTIX_TEST_PREVIOUS',
    '-I'+str(root/'.claude/worktrees/runtime-replacement-cursor-5445/src'),
    str(root/'.claude/worktrees/runtime-eliminate-limit-5450/tests/test_eliminate_limit.cpp'),
    *objects,'-o',str(target)],check=True)
result=subprocess.run([target],capture_output=True,text=True)
p=root/'benchmarks/runs/20260907-5450-eliminate-limit/logs/old-fixture.log'
assert not p.exists()
p.write_text(result.stdout+result.stderr)
print(result.stdout+result.stderr,end='')
assert result.returncode==0,result.returncode
