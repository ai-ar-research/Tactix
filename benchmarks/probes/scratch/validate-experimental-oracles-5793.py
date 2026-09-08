#!/usr/bin/env python3
"""Actual fixed benchmark-checker controls, not a solver or fuzz invocation."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s'
fixtures = Path(__file__).with_name('5793-experimental-oracle')
target = c / 'oracle-validation.json'
assert not target.exists()
sys.path.insert(0, str(c / 'pins'))
from runtime_task import checked_verdict_valid

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()

m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
records = []
for verdict, checker, cnf, extension in [('SAT', 'checkmodel', 'sat.cnf', 'model'),
                                         ('UNSAT', 'drat-trim', 'unsat.cnf', 'drat')]:
    for good in (True, False):
        artifact = fixtures / (('good' if good else 'bad') + '.' + extension)
        command = [str(c / 'pins' / checker), str(fixtures / cnf), str(artifact)]
        p = subprocess.run(command, capture_output=True, text=True, timeout=30)
        output = p.stdout + p.stderr
        verified = p.returncode == 0
        if verdict == 'UNSAT': verified &= 's VERIFIED' in output and 's NOT VERIFIED' not in output
        assert verified == good, (command, p.returncode, output)
        assert checked_verdict_valid('UNKNOWN', verdict,
            dict(exit_code=10 if verdict == 'SAT' else 20, timed_out=False), verified) == good
        records.append(dict(verdict=verdict, expected_acceptance=good, command=command,
                            exit_code=p.returncode, output=output, verified=verified))
out = dict(purpose='Fixed positive/negative actual benchmark checker controls; no solver execution or fuzz artifacts.',
           manifest_sha256=sha(c / 'manifest.json'), checks=records,
           fixture_sha256={str(p.relative_to(root)):sha(p) for p in fixtures.iterdir()},
           runner_sha256=sha(c / 'pins/runtime_task.py'))
target.write_text(json.dumps(out, indent=2) + '\n')
print('PASS: actual SAT and UNSAT checkers accept valid and reject invalid artifacts; UNKNOWN labels never bypass checking.')
