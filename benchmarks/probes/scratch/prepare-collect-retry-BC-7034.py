#!/usr/bin/env python3
"""Freeze full B28 plus the unchanged mechanism-relevant C5 before Tier-D."""
from pathlib import Path
import json

root = Path(__file__).resolve().parents[3]
proof = json.loads((root / 'benchmarks/runs/20260910-7033-collect-retry-proofs/summary.json').read_text())
assert proof['checked'] == 16 and proof['negative_controls'] == 2
source = Path(__file__).with_name('prepare-material-schedule-BC-6584.py')
code = source.read_text()
changes = {
    '20260909-6427-activation-stamp-BC-gate': '20260909-6737-reduce-watermark-BC-gate',
    '20260909-6424-activation-stamp-build': '20260909-6730-reduce-watermark-build',
    '20260909-6579-material-schedule-build': '20260910-7030-collect-retry-build',
    '20260909-6584-material-schedule-BC-gate': '20260910-7034-collect-retry-BC-gate',
    '20260909-6582-material-proof-control': '20260910-7033-collect-retry-proofs',
    "'6424-activation-stamp'": "'6730-reduce-watermark'",
    "'6579-material-schedule'": "'7030-collect-retry'",
    "'6114-substitute-publication-BC', '6584-material-schedule-BC'": "'6737-reduce-watermark-BC', '7034-collect-retry-BC'",
    'Approved canonical material-flag and Eliminate watermark cutover B28+C5 gate against immutable6424. Stage-one neutral control6574 is a dependency.':
        'Satisfied-large root-collection retry advice gate against immutable6730. Full B28 and inherited predeclared C5, no changes to old campaigns.',
    'New flags and the material counter recover missing native invitations and retain pending canonical work. Require all expected verdicts and independent proofs/models, no solve loss or unexplained termination; timing is diagnostic, not acceptance.':
        'Match Kissat sparse collection by suppressing retry/BCE/score removal notifications only for root-satisfied large retirement. Preserve Vivify/binary behavior and public rule availability. Require all expected verdicts and independent proofs/models; changed search work is expected, not a neutrality failure.',
    'benchmarks/runs/20260909-6574-material-state-BC-gate/neutrality-6576.json':
        'benchmarks/runs/20260910-7024-rank35-retry-producer/producer-localization-7032.json',
}
for i, (before, after) in enumerate(changes.items()):
    assert before in code, before
    code = code.replace(before, f'__REPLACEMENT_{i}__')
for i, after in enumerate(changes.values()):
    code = code.replace(f'__REPLACEMENT_{i}__', after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
