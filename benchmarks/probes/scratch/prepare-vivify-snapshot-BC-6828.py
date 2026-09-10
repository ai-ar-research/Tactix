#!/usr/bin/env python3
"""Freeze unchanged B28+C5 composition against actual6730 with exact-work gate."""
from pathlib import Path
source = Path(__file__).with_name('prepare-material-schedule-BC-6584.py')
code = source.read_text()
changes = {
    '20260909-6427-activation-stamp-BC-gate': '20260909-6737-reduce-watermark-BC-gate',
    '20260909-6424-activation-stamp-build': '20260909-6730-reduce-watermark-build',
    '20260909-6579-material-schedule-build': '20260910-6822-vivify-snapshot-build',
    '20260909-6584-material-schedule-BC-gate': '20260910-6828-vivify-snapshot-BC-gate',
    '20260909-6582-material-proof-control': '20260910-6827-vivify-snapshot-proofs',
    "'6424-activation-stamp'": "'6730-reduce-watermark'",
    "'6579-material-schedule'": "'6822-vivify-snapshot'",
    "'6114-substitute-publication-BC', '6584-material-schedule-BC'": "'6737-reduce-watermark-BC', '6828-vivify-snapshot-BC'",
    'Approved canonical material-flag and Eliminate watermark cutover B28+C5 gate against immutable6424. Stage-one neutral control6574 is a dependency.':
        'Same-work Vivify snapshot omission on current6730; user inprocessing-cost priority with scheduling/CDCL preserved. Existing campaigns untouched.',
    'New flags and the material counter recover missing native invitations and retain pending canonical work. Require all expected verdicts and independent proofs/models, no solve loss or unexplained termination; timing is diagnostic, not acceptance.':
        'Avoid copying unused literal snapshots only in pure-irredundant non-flushing schedules. Every pair must retain exact non-timing statistics and model/proof bytes; require all expected verdicts and independently valid certificates. Timing here is diagnostic.',
    'Changed work/certificates are expected, not a neutrality requirement.':
        'Exact current/candidate non-timing statistics and certificate bytes/hash required for all33 pairs; invalidity or unexplained work change pauses dependents.',
    'benchmarks/runs/20260909-6574-material-state-BC-gate/neutrality-6576.json':
        'benchmarks/runs/20260910-6824-vivify-snapshot-control/control-6824.json',
}
for i, (before, after) in enumerate(changes.items()):
    assert before in code, before
    code = code.replace(before, f'__REPLACEMENT_{i}__')
for i, after in enumerate(changes.values()):
    code = code.replace(f'__REPLACEMENT_{i}__', after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
