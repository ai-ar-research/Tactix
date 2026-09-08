#!/usr/bin/env python3
"""Compare actual native/Tactix continuation on the same six fixed inputs."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'benchmarks/runs/20260908-5798-forward-unit/native-controls-2'
out.mkdir(exist_ok=False)
build = root / 'build/pin-5798-forward-unit-release'
source = root / '.claude/worktrees/runtime-forward-unit-5798'
native = root / 'tools/solvers/variants/kissat-score-bump-observer-5696'
fixtures = root / 'benchmarks/probes/scratch'
link = shlex.split((build / 'tests/CMakeFiles/test_forward_unit.dir/link.txt').read_text())
objects = [(build / 'tests' / name).resolve() for name in link if name.endswith('.o')][1:]
assert objects and all(p.is_file() for p in objects)
commands = []
def run(label, command):
    commands.append([str(x) for x in command])
    r = subprocess.run(command, cwd=root, capture_output=True, text=True)
    (out / (label + '.log')).write_text(r.stdout + r.stderr)
    assert r.returncode == 0, (label, r.returncode, r.stdout, r.stderr)
    return r.stdout
run('compile-tactix', [link[0], '-O3', '-DNDEBUG', '-std=c++20', '-fno-exceptions', '-fno-rtti',
    '-I' + str(source / 'src'), '-I' + str(source / 'include'),
    fixtures / 'forward-unit-tactix-5802.cpp', *objects, '-o', out / 'tactix'])
run('compile-native', ['gcc', '-O3', '-DNDEBUG', '-I' + str(native / 'src'),
    fixtures / 'forward-unit-native-5802.c', native / 'build/libkissat.a', '-lm', '-o', out / 'native'])
tactix = run('tactix', [out / 'tactix'])
kissat = run('native', [out / 'native'])
assert len(tactix.splitlines()) == len(kissat.splitlines()) == 6
assert tactix == kissat, (tactix, kissat)
paths = [*objects, out / 'tactix', out / 'native', native / 'src/forward.c', native / 'build/libkissat.a',
    *(fixtures / name for name in ['forward-unit-tactix-5802.cpp', 'forward-unit-native-5802.c', 'forward-unit-cases-5802.h'])]
(out / 'summary.json').write_text(json.dumps(dict(cases=6, exact_rows_equal=True, rows=tactix.splitlines(),
    commands=commands, hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
    limitation='Fixed root-state/trail/forward-step/completion controls, not whole-formula equivalence or a real-instance runtime result.'), indent=2) + '\n')
print('PASS: six actual Kissat/Tactix continuation rows agree exactly')
