#!/usr/bin/env python3
"""Reproduce the fixture ABI error on its parent; validate the matched fixture."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6004-clause-pose-series-controls'
old = root / 'tools/solvers/variants/kissat-bve-repeat-heap-5985'
new = root / 'tools/solvers/variants/kissat-clause-pose-series-6004'
bad = Path(__file__).with_name('test-clause-pose-native-6006.c')
fixed = Path(__file__).with_name('test-clause-pose-native-fixed-6011.c')
target = c / 'summary.json'; assert not target.exists()
assert '100% tests passed out of 98' in (c / 'logs/tactix-ctest.log').read_text()
def compile_fixture(source, fixture, name):
    exe = c / name; assert not exe.exists()
    subprocess.run(['gcc', '-O2', '-I' + str(source / 'src'), str(fixture), str(source / 'build/libkissat.a'), '-lm', '-o', str(exe)], check=True)
    return exe
bad_parent = compile_fixture(old, bad, 'bad-parent-control')
r = subprocess.run([bad_parent, c / 'bad-parent.tsv', '3,1,-2'], capture_output=True, text=True)
assert r.returncode == -11, (r.returncode, r.stdout, r.stderr)
fixed_parent = compile_fixture(old, fixed, 'fixed-parent-control')
r = subprocess.run([fixed_parent, c / 'fixed-parent.tsv', '3,1,-2'], capture_output=True, text=True)
assert r.returncode == 0, (r.returncode, r.stdout, r.stderr)
exe = compile_fixture(new, fixed, 'fixed-native-control')
spec = importlib.util.spec_from_file_location('pose', Path(__file__).with_name('validate-clause-pose-series-6009.py'))
v = importlib.util.module_from_spec(spec); spec.loader.exec_module(v); v.fixtures()
cases = {}
for name, selected, expected in [('valid', '3,1,-2', 0), ('duplicate', '1,-1,3', 2),
                                ('trailing-comma', '1,-2,3,', 2), ('invalid-token', '1,x,3', 2),
                                ('missing', '', 2), ('too-small', '1,-2', 2)]:
    trace = c / ('fixed-' + name + '.tsv'); assert not trace.exists()
    r = subprocess.run([exe, trace, selected], capture_output=True, text=True)
    assert r.returncode == expected, (name, r.returncode, r.stdout, r.stderr)
    if expected == 0:
        with trace.open() as stream: frames = v.parse(stream, {1, -2, 3})
        assert len(frames) == 3 and [len(f['clauses']) for f in frames] == [2, 2, 0]
        assert all(f['conflicts'] == 1 and f['name'] == 'vivify' for f in frames)
        assert frames[1]['clauses'][0]['pose'] == [-2, 1, 3]
    cases[name] = dict(exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
target.write_text(json.dumps(dict(native_controls=cases, full_trace_suite='98/98 PASS',
    test_consideration='Actual Tactix/native boundary controls cover duplicates, changed pose, disappearance and name/window filters; five malformed selectors exit2. Full trace suite and strict parser pass. No solver behavior change.',
    initial_compile_failure='New Tactix fixture omitted explicit glue and passed Ref instead of Clause*; fixed before execution, initial log retained.',
    fixture_failure='The first native fixture undefines NDEBUG against a Release libkissat.a, changing struct kissat layout. Backtrace reached mark_removed_literals. The same fixture crashes with SIGSEGV on untouched5985 parent; matched-NDEBUG fixture exits0 on parent and new observer. No real solver run was submitted before closure.',
    parent_reproduction=dict(bad_exit=-11, fixed_exit=0),
    hashes={str(p.relative_to(root)): sha(p) for p in [exe, bad_parent, fixed_parent, bad, fixed, c / 'fixed-valid.tsv', *(c / 'logs').glob('*.log')]}), indent=2) + '\n')
print('PASS: inherited fixture ABI failure reproduced; parent/new repaired controls and98/98 suite verified')
