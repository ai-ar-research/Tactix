#!/usr/bin/env python3
"""Require exact B/C work and certificates before sealing a solver checkpoint."""
from pathlib import Path
source = Path(__file__).with_name('validate-material-schedule-BC-6586.py')
code = source.read_text()
changes = {
    '20260909-6584-material-schedule-BC-gate': '20260909-6766-watermark-layout-BC-gate',
    'summary-6586.json': 'summary-6768.json',
    "    pairs.append(dict(instance=a['instance']":
        "    assert work(a) == work(b), (task, 'layout changed work')\n    assert a['certificate_sha256'] == b['certificate_sha256'] and a['certificate_bytes'] == b['certificate_bytes'], (task, 'layout changed certificate')\n    pairs.append(dict(instance=a['instance']",
    'Full solver outcomes permit provisional continuation, not checked acceptance or performance promotion. Changed work is expected; all partial results retained. One timing wave only.':
        'All33 B/C pairs preserve exact non-timing statistics and certificate bytes/hashes. Expected outcomes allow provisional continuation; pending checks do not close acceptance. One timing wave only, no promotion or native competitiveness claim.'}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
