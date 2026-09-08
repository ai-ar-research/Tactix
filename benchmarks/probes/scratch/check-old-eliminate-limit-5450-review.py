#!/usr/bin/env python3
"""5450 fixture review: link the revised fixture against the immutable
predecessor objects.  Distinct from check-old-eliminate-limit-5450.py; it
owns only *-review* binaries and logs and never touches the retained ones.

  --future   omit -DTACTIX_TEST_PREVIOUS: the post-fix expectation, which the
             predecessor must FAIL at the optional-round assertion.
"""
from pathlib import Path
import shlex
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
build=root/'build/pin-5445-replacement-cursor-release'
command=shlex.split((build/'tests/CMakeFiles/test_slot_pose.dir/link.txt').read_text())
objects=[str((build/'tests'/arg).resolve()) for arg in command
    if arg.endswith('.o') and 'test_slot_pose.cpp.o' not in arg]
future='--future' in sys.argv[1:]
tag='eliminate-limit-old-5450-review'+('-future' if future else '')
target=root/'benchmarks/probes/scratch'/tag
defines=[] if future else ['-DTACTIX_TEST_PREVIOUS']
subprocess.run([command[0],'-std=c++20','-O3',*defines,
    '-I'+str(root/'.claude/worktrees/runtime-replacement-cursor-5445/src'),
    str(root/'.claude/worktrees/runtime-eliminate-limit-5450/tests/test_eliminate_limit.cpp'),
    *objects,'-o',str(target)],check=True)
result=subprocess.run([target],capture_output=True,text=True)
p=root/'benchmarks/runs/20260907-5450-eliminate-limit/logs'/(
    'old-fixture-review'+('-future-expectation' if future else '')+'.log')
p.write_text(result.stdout+result.stderr+f'exit={result.returncode}\n')
print(result.stdout+result.stderr,end='')
print(f'exit={result.returncode}')
sys.exit(0 if (result.returncode!=0)==future else 1)
