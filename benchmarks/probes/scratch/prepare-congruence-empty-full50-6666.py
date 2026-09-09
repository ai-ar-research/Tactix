#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('prepare-material-full50-6594.py')
code = source.read_text()
changes = {
 'benchmarks/runs/20260909-6584-material-schedule-BC-gate/summary-6586.json': 'benchmarks/runs/20260909-6661-congruence-empty-BC-gate/summary-6665.json',
 'benchmarks/runs/20260909-6587-rank50-material-schedule/summary-6589.json': 'benchmarks/runs/20260909-6658-rank49-congruence-empty-prefix/summary-6660.json',
 "w['observer_neutral']": "w['bounded_stop_clean']",
 "w['decision_difference']['matching_rows']==1459556 and w['boundary_comparison']['equal']": "w['decision_difference']['matching_rows']==618727 and w['full_large_rows_equal'] and w['binary_multisets_equal']",
 '20260909-6594-material-ranks1-50': '20260909-6666-congruence-empty-ranks1-50',
 '20260909-6579-material-schedule-build': '20260909-6654-congruence-empty-gates-build',
 'build/pin-6579-material-schedule-': 'build/pin-6654-congruence-empty-gates-',
 "causal_closures={'50'": "causal_closures={'49'",
 '20260909-6592-rank50-material-three-wave-runtime': '20260909-6663-rank49-congruence-empty-runtime',
 'Canonical subsumption-material lifecycle restores Rank50 invitation and search prefix. Measure every rank, including adverse changes and memory, without assuming aggregate benefit.':
    'Gate-free binary equivalence rounds now reach existing second dense cleanup. Rank49 firstProbe physical formula and100K decisions match native. Measure all50, including adverse work and memory, before accepting aggregate benefit.',
 'No source change, reference refresh, new tier selection or production mutation.': 'Isolated6654 source repair only; no reference refresh, new tier selection or production mutation.',
 '20260909-6439-activation-ranks1-50': '20260909-6594-material-ranks1-50',
}
for i, (before, after) in enumerate(changes.items()):
    assert before in code, before
    code = code.replace(before, f'__SUBSTITUTION_{i}__')
for i, after in enumerate(changes.values()): code = code.replace(f'__SUBSTITUTION_{i}__', after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
