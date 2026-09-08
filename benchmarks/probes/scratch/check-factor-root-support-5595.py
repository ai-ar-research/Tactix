#!/usr/bin/env python3
"""Compare actual old/new Factor index scores with the native connection routine."""
from pathlib import Path
import hashlib
import json
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5592-factor-root-support'
scratch = root / 'benchmarks/probes/scratch'
old = root / 'build/pin-5522-sweep-small-release'
new = root / 'build/pin-5592-factor-root-support-release'
source = root / '.claude/worktrees/runtime-factor-root-support-5592'
native = root / 'tools/solvers/variants/kissat-imply-fast-5414'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def run(name, command):
    p = c / 'logs' / (name + '.log')
    assert not p.exists()
    r = subprocess.run(command, cwd=root, capture_output=True, text=True)
    p.write_text(r.stdout + r.stderr)
    assert r.returncode == 0, (command, r.returncode, p)
    return r.stdout
command = shlex.split((old / 'tests/CMakeFiles/test_kernel.dir/link.txt').read_text())
objects = [str((old / 'tests' / arg).resolve()) for arg in command if arg.endswith('.o') and 'test_kernel.cpp.o' not in arg]
target = scratch / 'old-factor-root-support-5595'
assert not target.exists()
run('compile-old-fixture', [command[0], '-std=c++20', '-O3',
    '-DTACTIX_EXPECT_UNFILTERED_FACTOR_SUPPORT',
    '-I' + str(root / '.claude/worktrees/runtime-sweep-small-5522/src'),
    str(source / 'tests/test_factor_root_support.cpp'), *objects, '-o', str(target)])
before = run('old-fixture', [target]).splitlines()
after = run('new-fixture', [new / 'tests/test_factor_root_support']).splitlines()
reference = run('native-fixture', [scratch / 'reference-factor-root-support-5595']).splitlines()
assert before[-1] == after[-1] == 'PASS: 96 actual Factor support-count cases'
before, after = [list(map(int, row.split())) for row in before[:-1]], [list(map(int, row.split())) for row in after[:-1]]
reference = [list(map(int, row.split())) for row in reference]
assert len(before) == len(after) == 96 and len(reference) == 64
assert [r for r in after if r[3]] == reference
differences = []
for a, b in zip(before, after):
    assert a[:5] == b[:5]
    if a != b:
        assert a[0] == 0 and a[2] == 1 and a[3] > 0
        assert a[-1] == 2 and b[-1] == 1
        differences.append(dict(parameters=a[:5], previous=a[-1], candidate=b[-1]))
assert len(differences) == 16
paths = [source / 'src/rules/factor/factor.cpp', source / 'tests/test_factor_root_support.cpp',
    scratch / 'reference-factor-root-support-5595.c', scratch / 'reference-factor-root-support-5595',
    native / 'src/factor.c', native / 'build/libkissat.a', target,
    new / 'tests/test_factor_root_support', *(Path(p) for p in objects)]
record = dict(cases=96, native_cases=64, changes=differences,
    result='All64 positive-round actual native connection scores match the new Factor strategy-observed scores. Exactly16 old/new scores change when a root-fixed endpoint precedes the other endpoint in import order. Physical binaries, large clauses and stable root identities are preserved in all96 actual Tactix cases.',
    limitation='The native fixture calls its actual static connection routine by including pinned factor.c and linking its archive dependencies. It compares candidate support, not full native factoring. Tactix rounds0 intentionally disables its candidate prune and is tested as a Tactix control, not native parity. Existing suites own guard/N1/reconstruction/DRAT/LRAT coverage.',
    references='Kissat checks ACTIVE before counting the smaller endpoint; CaDiCaL counts physical binary clauses without this check. Preserve Kissat-default fidelity while leaving physical grid material and the public seed surface intact.',
    hashes={str(p.relative_to(root)):sha(p) for p in paths})
p = c / 'reference-validation.json'
assert not p.exists()
p.write_text(json.dumps(record, indent=2) + '\n')
print('PASS: 96 actual old/new cases, 16 targeted changes, 64 actual native connection cases')
