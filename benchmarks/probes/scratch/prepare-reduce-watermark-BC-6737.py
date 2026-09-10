#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('prepare-material-schedule-BC-6584.py')
code=source.read_text()
changes={
 '20260909-6427-activation-stamp-BC-gate':'20260909-6661-congruence-empty-BC-gate',
 '20260909-6424-activation-stamp-build':'20260909-6654-congruence-empty-gates-build',
 '20260909-6579-material-schedule-build':'20260909-6730-reduce-watermark-build',
 '20260909-6584-material-schedule-BC-gate':'20260909-6737-reduce-watermark-BC-gate',
 '20260909-6582-material-proof-control':'20260909-6734-reduce-watermark-proof-control',
 "'6424-activation-stamp'":"'6654-congruence-empty-gates'",
 "'6579-material-schedule'":"'6730-reduce-watermark'",
 "'6114-substitute-publication-BC', '6584-material-schedule-BC'":"'6661-congruence-empty-BC', '6737-reduce-watermark-BC'",
 'Approved canonical material-flag and Eliminate watermark cutover B28+C5 gate against immutable6424. Stage-one neutral control6574 is a dependency.':
 'Historical Reduce watermark B28+C5 gate against immutable6654; all prior campaigns untouched.',
 'New flags and the material counter recover missing native invitations and retain pending canonical work. Require all expected verdicts and independent proofs/models, no solve loss or unexplained termination; timing is diagnostic, not acceptance.':
 'Retain the old collection start through clause promotion, separately from the next-reduction marker. Require all expected verdicts and independently valid proofs/models; no solve loss or unexplained termination.',
 'benchmarks/runs/20260909-6574-material-state-BC-gate/neutrality-6576.json':
 'benchmarks/runs/20260909-6733-reduce-watermark-native/summary.json'}
for i,(before,after) in enumerate(changes.items()):
    assert before in code,before;code=code.replace(before,f'__SUB_{i}__')
for i,after in enumerate(changes.values()):code=code.replace(f'__SUB_{i}__',after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
