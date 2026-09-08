#!/usr/bin/env python3
"""Build a fixed scheduler witness against the unchanged5798 objects."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
out = root / 'benchmarks/runs' / sys.argv[1]
assert out.parent == root / 'benchmarks/runs' and out.name.startswith('20260908-')
out.mkdir(exist_ok=False)
source = root / '.claude/worktrees/runtime-forward-unit-5798'
build = root / 'build/pin-5798-forward-unit-release'
header = root / 'benchmarks/runs/20260908-5813-forward-unit-edges/forward-fixture.hpp'
fixture = root / 'benchmarks/probes/scratch/forward-caller-debt-5824.cpp'
assert header.read_text() == (source / 'tests/test_forward_unit.cpp').read_text().split('static void independent_tail(')[0]
link = shlex.split((build / 'tests/CMakeFiles/test_forward_unit.dir/link.txt').read_text())
objects = [(build / 'tests' / name).resolve() for name in link if name.endswith('.o')][1:]
commands = []
for name, command in [('compile', [link[0], '-O3', '-DNDEBUG', '-std=c++20', '-fno-exceptions', '-fno-rtti',
                       '-I' + str(source / 'src'), '-I' + str(source / 'include'), '-I' + str(header.parent),
                       fixture, *objects, '-o', out / 'witness']), ('witness', [out / 'witness'])]:
    commands.append(list(map(str, command)))
    r = subprocess.run(command, cwd=root, capture_output=True, text=True)
    (out / (name + '.log')).write_text(r.stdout + r.stderr)
    assert r.returncode == 0, (name, r.returncode, r.stdout, r.stderr)
    print(name, r.stdout or 'PASS', flush=True)
hashes = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in [fixture, header, *objects, out / 'witness']}
(out / 'summary.json').write_text(json.dumps(dict(commands=commands, hashes=hashes,
    result='Actual Eliminate scheduler exposes pending large-clause unit to next slot after partial forward quota cut;phase exit later repairs it. Fixed SAT model verified.',
    limitation='Diagnostic regression asserting current defect,not a repaired solver,proof differential,or fuzz coverage.'), indent=2) + '\n')
