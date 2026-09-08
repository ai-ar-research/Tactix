#!/usr/bin/env python3
"""Prove the old behavior with the actual immutable predecessor objects."""
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
build = root / 'build/pin-5473-sweep-cursor-release'
command = shlex.split((build / 'tests/CMakeFiles/test_kernel.dir/link.txt').read_text())
objects = [str((build / 'tests' / arg).resolve()) for arg in command
           if arg.endswith('.o') and 'test_kernel.cpp.o' not in arg]
target = root / 'benchmarks/probes/scratch/sweep-retained-old-5510'
source = root / '.claude/worktrees/runtime-sweep-retained-5510/tests/test_sweep_retained.cpp'
assert not target.exists()
subprocess.run([command[0], '-std=c++20', '-O3',
    '-I' + str(root / '.claude/worktrees/runtime-sweep-cursor-5473/src'),
    str(source), *objects, '-o', str(target)], check=True)
result = subprocess.run([target, '--old-counterexample'], capture_output=True,
                        text=True, check=True)
path = root / 'benchmarks/runs/20260908-5510-sweep-retained/logs/old-counterexample.log'
assert not path.exists()
path.write_text(result.stdout)
print(result.stdout, end='')
