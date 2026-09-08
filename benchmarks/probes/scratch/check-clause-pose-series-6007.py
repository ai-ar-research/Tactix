#!/usr/bin/env python3
"""Record successful rebuilds, full trace suite and actual native controls."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6004-clause-pose-series-controls'
build = root / 'build/pin-6004-clause-pose-series-trace'
native = root / 'tools/solvers/variants/kissat-clause-pose-series-6004'
def run(name, cmd, cwd=root):
    log = c / 'logs' / (name + '.log'); assert not log.exists()
    with log.open('w') as stream:
        r = subprocess.run(cmd, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT)
    assert r.returncode == 0, (name, log.read_text()[-6000:])
    print(name, 'PASS', flush=True)
run('tactix-build-retry', ['cmake', '--build', str(build), '-j8'])
run('kissat-build-retry', ['make', '-j8'], native)
run('tactix-ctest', ['ctest', '--test-dir', str(build), '--output-on-failure', '-j1'])
fixture = Path(__file__).with_name('test-clause-pose-native-6006.c')
exe = c / 'native-control'
run('native-control-build', ['gcc', '-O2', '-I' + str(native / 'src'), str(fixture), str(native / 'build/libkissat.a'), '-lm', '-o', str(exe)])
spec = importlib.util.spec_from_file_location('pose', Path(__file__).with_name('validate-clause-pose-series-6009.py'))
v = importlib.util.module_from_spec(spec); spec.loader.exec_module(v); v.fixtures()
cases = {}
for name, selected, expected in [('valid', '3,1,-2', 0), ('duplicate', '1,-1,3', 2),
                                ('trailing-comma', '1,-2,3,', 2), ('invalid-token', '1,x,3', 2),
                                ('missing', '', 2), ('too-small', '1,-2', 2)]:
    trace = c / (name + '.tsv'); assert not trace.exists()
    r = subprocess.run([exe, trace, selected], capture_output=True, text=True)
    assert r.returncode == expected, (name, r.returncode, r.stdout, r.stderr)
    if expected == 0:
        with trace.open() as stream: frames = v.parse(stream, {1, -2, 3})
        assert len(frames) == 3 and [len(f['clauses']) for f in frames] == [2, 2, 0]
        assert all(f['conflicts'] == 1 and f['name'] == 'vivify' for f in frames)
        assert frames[1]['clauses'][0]['pose'] == [-2, 1, 3]
    cases[name] = dict(exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
target = c / 'summary.json'; assert not target.exists()
target.write_text(json.dumps(dict(native_controls=cases, full_trace_suite='98/98 PASS',
    test_consideration='Actual Tactix/native clause and boundary calls cover duplicate multiplicity, pose changes, disappearance, name/window exclusion. Native malformed selector controls exit2. Strict parser rejects eight malformed frames. Diagnostic-only; no solver semantics change.',
    initial_compile_failure='Fixed new Tactix fixture call signatures before execution: explicit glue and Clause* deletion arguments. Original build log retained; no solver or oracle failure.',
    hashes={str(p.relative_to(root)): sha(p) for p in [exe, fixture, c / 'valid.tsv', *(c / 'logs').glob('*.log')]}), indent=2) + '\n')
assert '100% tests passed out of 98' in (c / 'logs/tactix-ctest.log').read_text()
print('PASS: full trace suite and six actual native controls')
