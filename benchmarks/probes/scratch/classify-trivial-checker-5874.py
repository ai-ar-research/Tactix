#!/usr/bin/env python3
"""Classify a checker exit-status false negative without changing frozen results."""
import hashlib
import json
import lzma
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s'
out = root / 'benchmarks/reports/2026-09-08-trivial-checker-status.json'
assert not out.exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((c / 'manifest.json').read_text())
checker = c / 'pins/drat-trim'
assert sha(checker) == m['pin_sha256']['pins/drat-trim']
fixtures = Path(__file__).with_name('5874-checker-status')
records = []
for name, good in [('empty', True), ('sat', False)]:
    command = [str(checker), str(fixtures / (name + '.cnf')), str(fixtures / 'empty.proof')]
    r = subprocess.run(command, capture_output=True, text=True, timeout=30)
    output = r.stdout + r.stderr
    assert r.returncode == 1
    assert ('c trivial UNSAT' in output and 's VERIFIED' in output) == good
    assert ('s NOT VERIFIED' in output) == (not good)
    records.append(dict(command=command, exit_code=r.returncode, output=output))
p = c / 'results/829.json'
r = json.loads(p.read_text())
assert not r['valid'] and r['arm'] == 'kissat' and r['solve']['exit_code'] == 20
assert r['proof_check']['exit_code'] == 1 and 'c trivial UNSAT\ns VERIFIED' in r['proof_check']['output_tail']
cnf = root / 'benchmarks/cache' / r['instance']
assert sha(cnf) == m['input_sha256'][r['instance']]
empty_line = None
with lzma.open(cnf, 'rt') as stream:
    for number, line in enumerate(stream, 1):
        if line.strip() == '0':
            empty_line = number
            break
assert empty_line is not None
source = root / 'drat-trim/drat-trim.c'
text = source.read_text()
assert 'int sts = ERROR;' in text and 'return (sts != UNSAT);' in text
assert 'if (size == 0 && !fileSwitchFlag) retvalue = UNSAT;' in text
out.write_text(json.dumps(dict(classification='Checker-status false negative,not rejected proof or solver soundness failure. Pinned drat-trim detects an input empty clause during parse,prints VERIFIED,but leaves sts=ERROR and exits1.',
    instance=r['instance'], input_empty_clause_line=empty_line, actual_controls=records,
    hashes={str(p.relative_to(root)): sha(p) for p in [p, cnf, checker, source, *fixtures.iterdir()]},
    disposition='Original829 record and entire ongoing cpu-q runner remain immutable and invalid/unscored under their original contract. Queue affected statuses for a new separately audited validation/repair campaign; do not reinterpret them silently or stop unrelated Tactix development.',
    test_consideration='Actual pinned checker positive empty-input and negative false-refutation controls both return1;status text distinguishes them. Original compressed input hash and explicit empty clause independently establish trivial UNSAT. No real solver execution.'), indent=2) + '\n')
print('PASS: input empty clause at line', empty_line, ';checker status bug reproduced with valid/invalid controls;frozen829 unchanged')
