#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('prepare-material-schedule-BC-6584.py')
code = source.read_text()
changes = {
    '20260909-6427-activation-stamp-BC-gate': '20260909-6584-material-schedule-BC-gate',
    '20260909-6424-activation-stamp-build': '20260909-6579-material-schedule-build',
    '20260909-6579-material-schedule-build': '20260909-6654-congruence-empty-gates-build',
    '20260909-6584-material-schedule-BC-gate': '20260909-6661-congruence-empty-BC-gate',
    '20260909-6582-material-proof-control': '20260909-6657-congruence-empty-proof-control',
    "'6424-activation-stamp'": "'6579-material-schedule'",
    "'6579-material-schedule'": "'6654-congruence-empty-gates'",
    "'6114-substitute-publication-BC', '6584-material-schedule-BC'": "'6584-material-schedule-BC', '6661-congruence-empty-BC'",
    'Approved canonical material-flag and Eliminate watermark cutover B28+C5 gate against immutable6424. Stage-one neutral control6574 is a dependency.':
        'Full B28+C5 gate for no-gates Congruence early-return repair against frozen6579. Production and old campaigns unchanged.',
    'New flags and the material counter recover missing native invitations and retain pending canonical work. Require all expected verdicts and independent proofs/models, no solve loss or unexplained termination; timing is diagnostic, not acceptance.':
        'Binary-only equivalence rounds reach the existing forward cleanup; units-only rounds retain lazy behavior. Require expected verdicts, independently valid proofs/models, no solve loss or unexplained termination.',
    'benchmarks/runs/20260909-6574-material-state-BC-gate/neutrality-6576.json':
        'benchmarks/runs/20260909-6651-rank49-congruence-stages/summary-6653.json',
}
# Simultaneous substitutions prevent predecessor names becoming candidate names.
for i, (before, after) in enumerate(changes.items()):
    assert before in code, before
    code = code.replace(before, f'__SUBSTITUTION_{i}__')
for i, after in enumerate(changes.values()): code = code.replace(f'__SUBSTITUTION_{i}__', after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
