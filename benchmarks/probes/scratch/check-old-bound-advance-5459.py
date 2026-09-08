#!/usr/bin/env python3
"""Validate predecessor expectations using actual immutable 5450 objects."""
from pathlib import Path
import shlex
import subprocess
root=Path(__file__).resolve().parents[3]
b=root/'build/pin-5450-eliminate-limit-release'
command=shlex.split((b/'tests/CMakeFiles/test_slot_pose.dir/link.txt').read_text())
objects=[str((b/'tests'/arg).resolve()) for arg in command if arg.endswith('.o') and 'test_slot_pose.cpp.o' not in arg]
binary=root/'benchmarks/probes/scratch/bound-advance-old-5459'
assert not binary.exists()
subprocess.run([command[0],'-std=c++20','-O3','-DTACTIX_TEST_PREVIOUS_RAMP',
 '-I'+str(root/'.claude/worktrees/runtime-eliminate-limit-5450/src'),
 str(root/'.claude/worktrees/runtime-bound-advance-5459/tests/test_bve_bound_cut.cpp'),*objects,'-o',str(binary)],check=True)
result=subprocess.run([binary],capture_output=True,text=True)
log=root/'benchmarks/runs/20260907-5459-bound-advance/logs/old-fixture.log'
assert not log.exists();log.write_text(result.stdout+result.stderr+f'exit={result.returncode}\n')
print(result.stdout+result.stderr,end='')
assert result.returncode==0
