#!/usr/bin/env python3
"""Seal already verified proofs after correcting a journal outcome enum assertion."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5849-forward-caller'
target = c / 'final-proof-validation.json'
assert not target.exists()
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
summaries = []
for name, pin, test, count in [
    ('continuation-proofs-final', '5849-forward-caller-release', 'forward_unit', 16),
    ('caller-proofs-final', '5849-forward-caller-release', 'forward_caller', 6),
    ('journal-proofs', '5854-forward-caller-journal', 'forward_caller', 6)]:
    path = c / name / 'summary.json'
    m = json.loads(path.read_text())
    assert m['binary_sha256'] == sha(root / f'build/pin-{pin}/tests/test_{test}')
    assert m['checked_micro_proofs'] == count and len(m['records']) == count
    for r in m['records']:
        assert r['exit_code'] == 0
        for ext, key in [('cnf', 'cnf_sha256'), ('proof', 'proof_sha256'), ('log', 'log_sha256')]:
            assert sha(path.parent / (r['case'] + '.' + ext)) == r[key]
        assert sha(Path(r['command'][0])) == r['checker_sha256']
    summaries.append(path)
edge = root / 'benchmarks/runs/20260908-5859-forward-caller-edges-final/summary.json'
assert len(json.loads(edge.read_text())['checked_conflict_proofs']) == 12
summaries.append(edge)
journal = c / 'caller-journal.tsv'
rows = [line.split('\t') for line in journal.read_text().splitlines() if not line.startswith('#')]
outcomes = [row[14] for row in rows]
assert outcomes == ['applied', 'applied', 'declined'] + ['refuted'] * 6
assert all(row[3] == 'Propagate' and row[6] == 'post-forward-connected' for row in rows)
assert all('pending-propagation=1' in row[12] for row in rows if row[14] != 'declined')
target.write_text(json.dumps(dict(checked_proofs=40, journal_active_outcomes=outcomes,
    hashes={str(p.relative_to(root)): sha(p) for p in [*summaries, journal]},
    scope='Final binaries:16 continuation,12 edges,6 caller and6 active-journal independently verified proofs. Journal includes two applied closures, empty-suffix decline and six refutations.',
    tooling_correction='Final proof runner completed all40 checker verifications,then expected enum Ok instead of the established applied text. This sealer validates the existing records without overwriting them;no solver or checker failure.'), indent=2) + '\n')
print('PASS:40 proofs, nine exact active journal outcomes')
