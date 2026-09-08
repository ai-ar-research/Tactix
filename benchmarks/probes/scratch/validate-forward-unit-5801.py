#!/usr/bin/env python3
"""Independent checkers for the actual C2 fixed continuation fixtures."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'benchmarks/runs/20260908-5798-forward-unit/fixed-proofs'
out.mkdir(exist_ok=False)
binary = root / 'build/pin-5798-forward-unit-release/tests/test_forward_unit'
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

run = subprocess.run([binary, out], cwd=root, capture_output=True, text=True)
(out / 'fixtures.log').write_text(run.stdout + run.stderr)
assert run.returncode == 0, (run.returncode, run.stdout, run.stderr)
records = []
for cnf in sorted(out.glob('*.cnf')):
    proof = cnf.with_suffix('.proof')
    kind = 'lrat' if cnf.stem.endswith('-lrat') else 'drat'
    checker = root / 'tools/checkers/bin' / ('lrat-check' if kind == 'lrat' else 'drat-trim')
    command = [str(checker), str(cnf), str(proof)]
    check = subprocess.run(command, cwd=root, capture_output=True, text=True)
    log = out / (cnf.stem + '.log')
    log.write_text(check.stdout + check.stderr)
    verified = 'c VERIFIED' if kind == 'lrat' else 's VERIFIED'
    assert check.returncode == 0 and verified in log.read_text(), (command, check.returncode, log.read_text())
    records.append(dict(case=cnf.stem, command=command, exit_code=check.returncode,
        cnf_sha256=sha(cnf), proof_sha256=sha(proof), checker_sha256=sha(checker),
        log_sha256=sha(log)))
assert len(records) == 16
(out / 'summary.json').write_text(json.dumps(dict(state_model_cases=17,
    checked_micro_proofs=16, binary_sha256=sha(binary), records=records,
    limitation='Fixed actual-code regressions only; not fuzz or real-instance proof/performance gates.'), indent=2) + '\n')
print('PASS: 17 state/model cases, 8 DRAT and 8 LRAT independently verified proofs')
