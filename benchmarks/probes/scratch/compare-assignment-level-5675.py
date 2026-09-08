#!/usr/bin/env python3
"""Compile the new fixed fixture against actual immutable predecessor headers/objects."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5672-assignment-level'
old = root / 'build/pin-propagation-cursor-repair-5647-release'
source = root / '.claude/worktrees/runtime-propagation-cursor-repair-5647'
fixture = root / '.claude/worktrees/runtime-assignment-level-5672/tests/test_assignment_level_bound.cpp'
directory = c / 'predecessor-fixture'
directory.mkdir(exist_ok=False)
link = shlex.split((old / 'tests/CMakeFiles/test_propagation_cursor.dir/link.txt').read_text())
objects = [(old / 'tests' / name).resolve() for name in link if name.endswith('.o')][1:]
assert objects and all(p.is_file() for p in objects)
compiler = link[0]
commands = [
    ('compile', [compiler, '-O3', '-DNDEBUG', '-std=c++20', '-Wall', '-Wextra',
        '-fno-exceptions','-fno-rtti','-I'+str(source/'src'),'-I'+str(source/'include'),
        '-c',str(fixture),'-o',str(directory/'fixture.o')]),
    ('link', [compiler, '-O3', str(directory/'fixture.o'), *map(str,objects), '-o',str(directory/'fixture')]),
    ('run', [str(directory/'fixture')]),
]
for label, command in commands:
    p = directory / (label+'.log')
    with p.open('w') as stream:
        r = subprocess.run(command,cwd=root,stdout=stream,stderr=subprocess.STDOUT)
    assert r.returncode == 0,(label,p.read_text())
    print(label+': PASS',flush=True)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
(directory/'manifest.json').write_text(json.dumps(dict(
    commands=[list(map(str,cmd)) for _,cmd in commands],
    fixture_sha256=sha(fixture),predecessor_header_sha256=sha(source/'src/kernel/kernel.hpp'),
    predecessor_objects={str(p.relative_to(root)):sha(p) for p in objects},
    output=(directory/'run.log').read_text(),
    limitation='Fixed synthetic states,not real CNF benchmark;candidate output must agree before submission.'),indent=2)+'\n')
print((directory/'run.log').read_text())
