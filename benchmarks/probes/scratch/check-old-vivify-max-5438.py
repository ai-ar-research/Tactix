#!/usr/bin/env python3
"""Exercise the identical fixed scheduler fixture against immutable old objects."""
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
build = root / 'build/pin-5430-inprocess-root-release'
command = shlex.split((build / 'tests/CMakeFiles/test_vivify.dir/link.txt').read_text())
objects = [str((build / 'tests' / arg).resolve()) for arg in command
           if arg.endswith('.o') and 'test_vivify.cpp.o' not in arg]
target = root / 'benchmarks/probes/scratch/vivify-max-old-5438'
source = root / '.claude/worktrees/runtime-vivify-max-5438/tests/test_vivify_max.cpp'
subprocess.run([command[0], '-std=c++20', '-O3',
    '-I' + str(root / '.claude/worktrees/runtime-inprocess-root-5430/src'),
    str(source), *objects, '-o', str(target)], check=True)
result = subprocess.run([target], capture_output=True, text=True, check=True)
p = root / 'benchmarks/runs/20260907-5438-vivify-max/logs/old-fixture.log'
assert not p.exists()
p.write_text(result.stdout)
print(result.stdout, end='')
