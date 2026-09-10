#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('prepare-material-full50-6594.py')
code=source.read_text()
changes={
 'benchmarks/runs/20260909-6584-material-schedule-BC-gate/summary-6586.json':'benchmarks/runs/20260909-6737-reduce-watermark-BC-gate/summary-6740.json',
 'benchmarks/runs/20260909-6587-rank50-material-schedule/summary-6589.json':'benchmarks/runs/20260909-6735-reduce-watermark-prefix/summary-6739.json',
 "w['observer_neutral']":"w['bounded_stop_clean']",
 "w['decision_difference']['matching_rows']==1459556 and w['boundary_comparison']['equal']":"w['decision_difference']['matching_rows']==2167922 and w['boundary_repaired']",
 '20260909-6594-material-ranks1-50':'20260909-6743-reduce-watermark-ranks1-50',
 '20260909-6579-material-schedule-build':'20260909-6730-reduce-watermark-build',
 'build/pin-6579-material-schedule-':'build/pin-6730-reduce-watermark-',
 "causal_closures={'50'":"causal_closures={'46'",
 '20260909-6592-rank50-material-three-wave-runtime':'20260909-6741-rank46-watermark-runtime',
 'Canonical subsumption-material lifecycle restores Rank50 invitation and search prefix. Measure every rank, including adverse changes and memory, without assuming aggregate benefit.':'Historical Reduce boundary repair closes Rank46 full1M decision stream. Measure all50 including adverse work, runtime and memory; do not infer aggregate improvement from one prefix.',
 'No source change, reference refresh, new tier selection or production mutation.':'Isolated6730 repair only; no reference refresh, new tier selection or production mutation.',
 '20260909-6439-activation-ranks1-50':'20260909-6666-congruence-empty-ranks1-50'}
for i,(before,after) in enumerate(changes.items()):
    assert before in code,before;code=code.replace(before,f'__SUB_{i}__')
for i,after in enumerate(changes.values()):code=code.replace(f'__SUB_{i}__',after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
