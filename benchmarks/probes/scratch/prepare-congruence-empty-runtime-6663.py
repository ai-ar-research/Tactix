#!/usr/bin/env python3
"""Predeclare three paired Rank49 waves, using fixed6579 as the local control."""
from pathlib import Path
source = Path(__file__).with_name('prepare-material-runtime-6592.py')
code = source.read_text()
changes = {
 '20260909-6592-rank50-material-three-wave-runtime': '20260909-6663-rank49-congruence-empty-runtime',
 '20260909-6584-material-schedule-BC-gate': '20260909-6661-congruence-empty-BC-gate',
 'summary-6586.json': 'summary-6665.json',
 'benchmarks/runs/20260909-6587-rank50-material-schedule/summary-6589.json': 'benchmarks/runs/20260909-6658-rank49-congruence-empty-prefix/summary-6660.json',
 "w['observer_neutral']": "w['bounded_stop_clean']",
 '20260909-6439-activation-ranks1-50': '20260909-6594-material-ranks1-50',
 'results/049.json': 'results/048.json',
 "r['rank']==50": "r['rank']==49",
 "('candidate','6579-material-schedule'),('previous','6424-activation-stamp')": "('candidate','6654-congruence-empty-gates'),('previous','6579-material-schedule')",
 "rank=50,tier='D',instance=r['instance'],expected='SAT'": "rank=49,tier='D',instance=r['instance'],expected='SAT'",
 "'6584-material-schedule-BC','6592-rank50-material-runtime'": "'6661-congruence-empty-BC','6663-rank49-empty-runtime'",
 'Price the successful general Rank50 material scheduling repair against its immutable6424 control.': 'Price the general no-gates Congruence cleanup repair against immutable6579 on Rank49. Fixed Kissat runtime22.555s is not rerun or overwritten.',
 'Restoring the823562 material invitation reduces full Rank50 search work and runtime. No absolute gain assumed from prefix equality.': 'Restoring second dense cleanup gives exact native100K decisions and firstProbe formula. Measure full search/runtime; prefix agreement alone is not a gain.',
 'previous arm exact to6439': 'previous arm exact to6594',
}
for i, (before, after) in enumerate(changes.items()):
    assert before in code, before
    code = code.replace(before, f'__SUBSTITUTION_{i}__')
for i, after in enumerate(changes.values()): code = code.replace(f'__SUBSTITUTION_{i}__', after)
# Full-runtime measurement can overlap the still-running B/C solver pair;
# its acceptance still requires that entire gate. Fixed micro proofs and the
# causal prefix already passed, and no published B/C invalidity is allowed.
anchor = "gate=json.loads((old/'summary-6665.json').read_text()); assert gate['solver_records']==66"
assert code.count(anchor) == 1
code = code.replace(anchor, "proof=root/'benchmarks/runs/20260909-6657-congruence-empty-proof-control/summary.json'\nassert json.loads(proof.read_text())['final_proofs_checked']==64\nfor folder in ('provisional','results'):\n    for path in (old/folder).glob('*.json'):\n        state=classify(json.loads(path.read_text()),folder=='results')\n        assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(path,state)")
anchor = "(old/'summary-6665.json',witness)"
assert code.count(anchor) == 1
code = code.replace(anchor, '(proof,witness)')
code = code.replace("acceptance='All6", "acceptance='Full B/C66 checked/clean remains required; these experimental timing waves may overlap it. All6")
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
