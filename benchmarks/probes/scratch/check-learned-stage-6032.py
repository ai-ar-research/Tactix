#!/usr/bin/env python3
"""Validate actual native callbacks, large rows, and malformed windows."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6030-learned-stage-controls'
native = root / 'tools/solvers/variants/kissat-learned-stage-6030'
assert '100% tests passed out of 100' in (c / 'logs/tactix-ctest.log').read_text()
exe = c / 'native-control'; assert not exe.exists()
source = Path(__file__).with_name('test-learned-stage-native-6032.c')
subprocess.run(['gcc', '-O2', '-I'+str(native/'src'), source, native/'build/libkissat.a', '-lm', '-o', exe], check=True)
spec = importlib.util.spec_from_file_location('ls', Path(__file__).with_name('validate-learned-stage-6033.py'))
v = importlib.util.module_from_spec(spec); spec.loader.exec_module(v); v.fixtures()
cases = {}
for name, lower, upper, expected in [('valid','1','2',0), ('reversed','2','1',2), ('negative','-1','2',2),
    ('trailing','1x','2',2), ('empty','','2',2), ('overflow','18446744073709551616','2',2)]:
    trace = c / (name+'.tsv'); assert not trace.exists()
    r = subprocess.run([exe, trace, lower, upper], capture_output=True, text=True)
    assert r.returncode == expected, (name,r.returncode,r.stdout,r.stderr)
    if expected == 0:
        with trace.open() as stream: rows = v.parse(stream, 1, 2)
        assert len(rows) == 6 and [x['event'] for x in rows] == ['learned_raw','learned_sorted','learned_shrunk','learned','learned_raw','learned_shrunk']
        assert [x['literals'] for x in rows[:4]] == [[1,-2,3],[-2,1,3],[1,3],[1,3]]
        assert rows[4]['literals'] == list(range(1,2049)) and rows[5]['literals'] == []
    cases[name] = dict(exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
target = c / 'summary.json'; assert not target.exists()
target.write_text(json.dumps(dict(full_trace_suite='100/100 PASS',native_controls=cases,
    test_consideration='Actual Tactix/native learning callbacks cover inclusive endpoints, final-stage dispatch, empty and2048-literal clauses. Five malformed windows exit2; parser rejects eight malformed rows. Read-only diagnostic, no solver behavior change.',
    hashes={str(p.relative_to(root)):sha(p) for p in [exe,source,c/'valid.tsv',*(c/'logs').glob('*.log')]}),indent=2)+'\n')
print('PASS:100/100 trace suite and six actual native controls')
