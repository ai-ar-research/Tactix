#!/usr/bin/env python3
"""Validate actual native cache lifecycle and malformed selectors."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6016-vivify-pose-controls'
native = root / 'tools/solvers/variants/kissat-vivify-pose-6016'
assert '100% tests passed out of 99' in (c / 'logs/tactix-ctest.log').read_text()
exe = c / 'native-control'; assert not exe.exists()
source = Path(__file__).with_name('test-vivify-pose-native-6018.c')
subprocess.run(['gcc', '-O2', '-I'+str(native/'src'), source, native/'build/libkissat.a', '-lm', '-o', exe], check=True)
spec = importlib.util.spec_from_file_location('vp', Path(__file__).with_name('validate-vivify-pose-6019.py'))
v = importlib.util.module_from_spec(spec); spec.loader.exec_module(v); v.fixtures()
cases = {}
for name, selected, expected in [('valid','3,1,-2',0), ('duplicate','1,-1,3',2), ('trailing','1,-2,3,',2), ('bad-token','1,x,3',2), ('empty','',2), ('short','1,-2',2)]:
    trace = c / (name+'.tsv'); assert not trace.exists()
    r = subprocess.run([exe, trace, selected], capture_output=True, text=True)
    assert r.returncode == expected, (name,r.returncode,r.stdout,r.stderr)
    if expected == 0:
        with trace.open() as stream: rows, pending = v.parse(stream, {1,-2,3}, 1)
        assert len(rows) == 5 and pending == rows[-1]
        assert [r['pose'] for r in rows] == [[1,-2,3],[-2,1,3],[],[3,1,-2],[3,1,-2]]
        assert rows[-1]['scope'] == 2 and rows[-1]['ordinal'] == 0
    cases[name] = dict(exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
target = c / 'summary.json'; assert not target.exists()
target.write_text(json.dumps(dict(full_trace_suite='99/99 PASS',native_controls=cases,
    test_consideration='Actual Tactix/native callbacks cover selected-pose cache, garbage invalidation, replacement and scope reset; native uses matching Release layout. Five malformed selectors exit2; strict parser rejects eight malformed rows. Diagnostic-only, no solver behavior change.',
    hashes={str(p.relative_to(root)):sha(p) for p in [exe,source,c/'valid.tsv',*(c/'logs').glob('*.log')]}),indent=2)+'\n')
print('PASS:99/99 trace suite and six actual native controls')
