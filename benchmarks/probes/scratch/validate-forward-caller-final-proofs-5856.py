#!/usr/bin/env python3
"""Bind the final rebuild and explicitly enable the caller's journal path."""
import hashlib
import json
import os
from pathlib import Path

here = Path(__file__).resolve().parent
root = here.parents[2]
script = here / 'validate-forward-caller-proofs-5852.py'
code = script.read_text().replace('/continuation-proofs', '/continuation-proofs-final')
code = code.replace('/caller-proofs', '/caller-proofs-final')
code = code.replace('20260908-5853-forward-caller-edges', '20260908-5859-forward-caller-edges-final')
exec(compile(code, str(script), 'exec'), {'__file__': str(script), '__name__': '__main__'})
c = root / 'benchmarks/runs/20260908-5849-forward-caller'
journal = c / 'caller-journal.tsv'
assert not journal.exists()
keys = ['TACTIX_RULE_JOURNAL', 'TACTIX_RULE_JOURNAL_RULE', 'TACTIX_RULE_JOURNAL_CANDIDATE']
assert not any(key in os.environ for key in keys)
os.environ.update(TACTIX_RULE_JOURNAL=str(journal), TACTIX_RULE_JOURNAL_RULE='Propagate',
                  TACTIX_RULE_JOURNAL_CANDIDATE='post-forward-connected')
script = here / 'validate-forward-unit-5801.py'
code = script.read_text()
for old, new in {
    '20260908-5798-forward-unit/fixed-proofs': '20260908-5849-forward-caller/journal-proofs',
    'pin-5798-forward-unit-release/tests/test_forward_unit': 'pin-5854-forward-caller-journal/tests/test_forward_caller',
    'len(records) == 16': 'len(records) == 6', 'state_model_cases=17': 'state_model_cases=3',
    'checked_micro_proofs=16': 'checked_micro_proofs=6',
    '17 state/model cases, 8 DRAT and 8 LRAT': '3 state/model cases, 3 DRAT and 3 LRAT'}.items():
    assert old in code
    code = code.replace(old, new)
try:
    exec(compile(code, str(script), 'exec'), {'__file__': str(script), '__name__': '__main__'})
finally:
    for key in keys: del os.environ[key]
rows = [line.split('\t') for line in journal.read_text().splitlines() if not line.startswith('#')]
assert [row[14] for row in rows] == ['applied', 'applied', 'declined'] + ['refuted'] * 6, rows
assert all(row[3] == 'Propagate' and row[6] == 'post-forward-connected' for row in rows)
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
summaries = [c / (name + '/summary.json') for name in
             ['continuation-proofs-final', 'caller-proofs-final', 'journal-proofs']]
summaries.append(root / 'benchmarks/runs/20260908-5859-forward-caller-edges-final/summary.json')
target = c / 'final-proof-validation.json'
assert not target.exists()
target.write_text(json.dumps(dict(checked_proofs=40,
    journal_active_outcomes=[row[14] for row in rows],
    hashes={str(p.relative_to(root)): sha(p) for p in [*summaries, journal]},
    scope='Final binaries:16 inherited continuation,12 inherited edge,6 new caller and6 active-journal independently verified proofs. Existing checker invocation and acceptance contract unchanged.'), indent=2) + '\n')
print('PASS:40 independent proofs; caller journal path active')
