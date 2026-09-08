#!/usr/bin/env python3
"""Run the same fixed transition fixture against immutable 5438 objects."""
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
build = root / 'build/pin-5438-vivify-max-release'
command = shlex.split((build / 'tests/CMakeFiles/test_kernel.dir/link.txt').read_text())
objects = [str((build / 'tests' / arg).resolve()) for arg in command
           if arg.endswith('.o') and 'test_kernel.cpp.o' not in arg]
target = root / 'benchmarks/probes/scratch/replacement-cursor-old-5445'
source = root / '.claude/worktrees/runtime-replacement-cursor-5445/tests/test_replacement_cursor.cpp'
assert not target.exists()
subprocess.run([command[0], '-std=c++20', '-O3',
    '-I' + str(root / '.claude/worktrees/runtime-vivify-max-5438/src'),
    str(source), *objects, '-o', str(target)], check=True)
result = subprocess.run([target], capture_output=True, text=True, check=True)
p = root / 'benchmarks/runs/20260907-5445-replacement-cursor/logs/old-fixture.log'
assert not p.exists()
p.write_text(result.stdout)
print(result.stdout, end='')
