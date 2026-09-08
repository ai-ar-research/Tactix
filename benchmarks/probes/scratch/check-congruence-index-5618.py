#!/usr/bin/env python3
"""Record actual current/native fixed-fixture behavior, not a Python algorithm model."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
root = Path(__file__).resolve().parents[3]
scratch = root / 'benchmarks/probes/scratch'
c = root / 'benchmarks/runs/20260908-5618-congruence-index-fixture'
c.mkdir(exist_ok=True)
assert not (c / 'summary.json').exists(), 'Completed fixture is immutable'
(c / 'logs').mkdir(exist_ok=True)
build = root / 'build/pin-5592-factor-root-support-release'
native = root / 'tools/solvers/variants/kissat-imply-fast-5414'
command = shlex.split((build / 'tests/CMakeFiles/test_kernel.dir/link.txt').read_text())
objects = [str((build / 'tests' / x).resolve()) for x in command if x.endswith('.o') and 'test_kernel.cpp.o' not in x]
def run(name, args):
    r = subprocess.run(args, cwd=root, capture_output=True, text=True)
    p = c / 'logs' / (name + '.log')
    attempt = 0
    while p.exists():
        attempt += 1
        p = c / 'logs' / (name + '.r' + str(attempt) + '.log')
    p.write_text(r.stdout + r.stderr)
    assert r.returncode == 0, (name, r.returncode)
    return r.stdout
native_bin, current_bin = c / 'native', c / 'current'
run('compile-native', ['gcc', '-O3', '-DNDEBUG', '-I' + str(native / 'src'),
    str(scratch / 'reference-congruence-index-5618.c'), str(native / 'build/libkissat.a'), '-lm', '-o', str(native_bin)])
run('compile-current', [command[0], '-std=c++20', '-O3',
    '-I' + str(root / '.claude/worktrees/runtime-factor-root-support-5592/src'),
    str(scratch / 'current-congruence-index-5618.cpp'), *objects, '-o', str(current_bin)])
results = {arm: [list(map(int, line.split())) for line in run(arm, [path]).splitlines()]
           for arm, path in [('native', native_bin), ('current', current_bin)]}
assert len(results['native']) == len(results['current']) == 75
differences = []
for a,b in zip(results['current'], results['native']):
    assert a[:3] == b[:3] and a[3] == 0
    if a != b:
        assert a[2] == 1 and b[3] == 1
        differences.append(dict(parameters=a[:3], current_garbage=a[3], native_garbage=b[3]))
assert differences
paths = [native_bin, current_bin, scratch / 'reference-congruence-index-5618.c',
    scratch / 'current-congruence-index-5618.cpp', native / 'src/congruence.c', native / 'build/libkissat.a', *(Path(p) for p in objects)]
record = dict(cases=75, differences=differences,
    result='Actual current full no-gate Congruence applications retain all75 clauses; actual native XOR candidate indexing deletes the recorded root-satisfied cases. Pure public extraction remains read-only.',
    limitation='Fixed mechanism comparison, not a native full solve, not proof of the Rank33 discrepancy or runtime impact. False/unassigned controls and effective-size/physical-position boundary cases are retained. No solver source or pin changed.',
    hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
(c / 'summary.json').write_text(json.dumps(record, indent=2) + '\n')
print('PASS:75 actual current/native cases;', len(differences), 'root-satisfied lifecycle differences')
