#!/usr/bin/env python3
"""Actual native/SubSolver small-clause parity and old Sweep counterexample."""
from pathlib import Path
import hashlib
import json
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
scratch = root / 'benchmarks/probes/scratch'
campaign = root / 'benchmarks/runs/20260908-5522-sweep-small'
old = root / 'build/pin-5510-sweep-retained-release'
command = shlex.split((old / 'tests/CMakeFiles/test_kernel.dir/link.txt').read_text())
objects = [str((old / 'tests' / arg).resolve()) for arg in command
           if arg.endswith('.o') and 'test_kernel.cpp.o' not in arg]
native = root / 'tools/solvers/variants/kissat-imply-fast-5414'
def run(name, command):
    result = subprocess.run(command, cwd=root, text=True, capture_output=True, check=True)
    path = campaign / 'logs' / (name + '.log')
    assert not path.exists()
    path.write_text(result.stdout + result.stderr)
    return result.stdout
target = scratch / 'small-tactix-reference-5525'
assert not target.exists()
run('compile-tactix-reference', [command[0], '-std=c++20', '-O3',
    '-I' + str(root / '.claude/worktrees/runtime-sweep-retained-5510/src'),
    str(scratch / 'reference-small-tactix-5525.cpp'), *objects, '-o', str(target)])
candidate = run('tactix-reference', [target])
reference = run('native-reference', [scratch / 'reference-small-kitten-5525'])
assert candidate == reference
assert len(candidate.splitlines()) == 12
counterexample = scratch / 'small-sweep-old-5525'
assert not counterexample.exists()
run('compile-old-counterexample', [command[0], '-std=c++20', '-O3',
    '-I' + str(root / '.claude/worktrees/runtime-sweep-retained-5510/src'),
    str(root / '.claude/worktrees/runtime-sweep-small-5522/tests/test_sweep_small_env.cpp'),
    *objects, '-o', str(counterexample)])
result = run('old-counterexample', [counterexample, '--old-counterexample'])
assert 'old counterexample: all one-clause environments skipped' in result
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
paths = [scratch / 'reference-small-kitten-5525.c', scratch / 'reference-small-tactix-5525.cpp',
         native / 'build/libkissat.a', scratch / 'reference-small-kitten-5525', target,
         counterexample, *(Path(path) for path in objects)]
record = dict(cases=12, fields='width, repetition, initial ticks, post-flip ticks, flips, generator, model hash',
              result='byte-identical native and actual5510 SubSolver output; actual5510 Sweep skips all four one-clause fixtures',
              limitation='Fixed engine lifecycle comparison, not a full native Sweep or CaDiCaL run.',
              hashes={str(path.relative_to(root)): sha(path) for path in paths})
path = campaign / 'reference-validation.json'
assert not path.exists()
path.write_text(json.dumps(record, indent=2) + '\n')
print('PASS: 12 native/SubSolver one-clause lifecycle cases and old Sweep counterexample')
