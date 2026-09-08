#!/usr/bin/env python3
"""Run the actual old scheduler with successful assertions for its missing closure."""
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
build = root / 'build/pin-5419-rephase-root-release'
command = shlex.split((build / 'tests/CMakeFiles/test_rephase.dir/link.txt').read_text())
objects = [str((build / 'tests' / arg).resolve()) for arg in command if arg.endswith('.o') and 'test_rephase.cpp.o' not in arg]
target = root / 'benchmarks/probes/scratch/inprocess-root-old-5430'
source = root / '.claude/worktrees/runtime-inprocess-root-5430/tests/test_inprocess_root.cpp'
subprocess.run([command[0], '-std=c++20', '-O3', '-DTACTIX_EXPECT_OLD_ENTRY=1',
    '-I' + str(root / '.claude/worktrees/runtime-rephase-root-5417/src'), str(source),
    *objects, '-o', str(target)], check=True)
result = subprocess.run([target], capture_output=True, text=True, check=True)
dest = root / 'benchmarks/runs/20260907-5430-inprocess-root/logs/old-entry-fixture.log'
assert not dest.exists()
dest.write_text(result.stdout)
print(result.stdout, end='')
