#!/usr/bin/env python3
"""Strict B/C same-work checkpoint; certificate checks stay asynchronous."""
from pathlib import Path
source = Path(__file__).with_name('validate-material-schedule-BC-6586.py')
code = source.read_text()
for before, after in {
    '20260909-6584-material-schedule-BC-gate': '20260910-6828-vivify-snapshot-BC-gate',
    'summary-6586.json': 'summary-6830.json',
    "    pairs.append(dict(instance=a['instance']":
        "    assert work(a) == work(b), (task, 'snapshot omission changed work')\n    assert a['certificate_sha256'] == b['certificate_sha256'] and a['certificate_bytes'] == b['certificate_bytes'], (task, 'snapshot omission changed certificate')\n    pairs.append(dict(instance=a['instance']",
    'Full solver outcomes permit provisional continuation, not checked acceptance or performance promotion. Changed work is expected; all partial results retained. One timing wave only.':
        'All33 B/C pairs preserve exact non-timing statistics and certificate bytes/hashes. Expected outcomes permit provisional continuation, not checked acceptance or performance promotion. One diagnostic timing wave only; pending proofs remain unverified.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
