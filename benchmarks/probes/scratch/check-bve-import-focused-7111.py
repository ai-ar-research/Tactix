#!/usr/bin/env python3
"""Fully instrumented direct engine import regression before full builds."""
import json
import os
from pathlib import Path
import resource
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-bve-import-7110'
out = root / 'benchmarks/runs/20260910-7110-bve-import-build/focused'
assert not out.exists(); out.mkdir()
binary = out / 'test_subsolver'
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',
           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
commands = [
    ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O1', '-g',
     '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-exceptions', '-fno-rtti',
     '-I' + str(source / 'src'), source / 'tests/test_subsolver.cpp',
     source / 'src/engine/subsolver.cpp', '-o', binary],
    [binary]]
checks = []; hashes = {}
def no_core(): resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
for i, command in enumerate(commands):
    command = list(map(str, command)); log = out / f'{i}.log'
    with log.open('x') as stream:
        p = subprocess.run(command, cwd=root, env=env, stdout=stream,
                           stderr=subprocess.STDOUT, preexec_fn=no_core)
    checks.append(dict(command=command, exit_code=p.returncode,
                       log=str(log.relative_to(root)), sha256=sha(log)))
    assert p.returncode == 0, log
    print(i, 'PASS', flush=True)
for path in (binary, Path(__file__), source / 'tests/test_subsolver.cpp',
             source / 'src/engine/subsolver.cpp', source / 'src/engine/subsolver.hpp'):
    hashes[str(path.relative_to(root))] = sha(path)
assert 'mapped import: 252 fixed cases' in (out / '1.log').read_text()
with (out / 'summary.json').open('x') as stream:
    json.dump(dict(status='PASS', checks=checks, hashes=hashes,
        sanitizer_options={k: env[k] for k in ('ASAN_OPTIONS', 'UBSAN_OPTIONS')},
        scope='Full existing SubSolver test plus252 mapped-view/copy differential cases, independent brute-force/models/LRAT replay, resets, budgets and core refinement. No fuzz campaign or real-instance solving.'), stream, indent=2)
print(out / 'summary.json', flush=True)
