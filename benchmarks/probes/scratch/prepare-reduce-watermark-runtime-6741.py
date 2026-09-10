#!/usr/bin/env python3
"""Predeclare three full Rank46 pairs while B/C proof checks continue."""
from pathlib import Path
source=Path(__file__).with_name('prepare-material-runtime-6592.py')
code=source.read_text()
changes={
 '20260909-6592-rank50-material-three-wave-runtime':'20260909-6741-rank46-watermark-runtime',
 '20260909-6584-material-schedule-BC-gate':'20260909-6737-reduce-watermark-BC-gate',
 'summary-6586.json':'summary-6740.json',
 'benchmarks/runs/20260909-6587-rank50-material-schedule/summary-6589.json':'benchmarks/runs/20260909-6735-reduce-watermark-prefix/summary-6739.json',
 "w['observer_neutral']":"w['bounded_stop_clean'] and w['boundary_repaired']",
 '20260909-6439-activation-ranks1-50':'20260909-6666-congruence-empty-ranks1-50',
 'results/049.json':'results/045.json',"r['rank']==50":"r['rank']==46",
 "r['expected']=='SAT'":"r['expected']=='UNSAT'",
 "('candidate','6579-material-schedule'),('previous','6424-activation-stamp')":"('candidate','6730-reduce-watermark'),('previous','6654-congruence-empty-gates')",
 "rank=50,tier='D',instance=r['instance'],expected='SAT'":"rank=46,tier='D',instance=r['instance'],expected='UNSAT'",
 "'6584-material-schedule-BC','6592-rank50-material-runtime'":"'6737-reduce-watermark-BC','6741-rank46-watermark-runtime'",
 'Price the successful general Rank50 material scheduling repair against its immutable6424 control.':'Measure historical Reduce watermark repair against immutable6654 on Rank46; fixed Kissat80.043s is not rerun or overwritten.',
 'Restoring the823562 material invitation reduces full Rank50 search work and runtime. No absolute gain assumed from prefix equality.':'Restoring the historical collection cut closes all2167922 native decisions through1M conflicts. Measure full work and time without assuming a full-trajectory or runtime gain.',
 'previous arm exact to6439':'previous arm exact to6666'}
for i,(before,after) in enumerate(changes.items()):
    assert before in code,before;code=code.replace(before,f'__SUB_{i}__')
for i,after in enumerate(changes.values()):code=code.replace(f'__SUB_{i}__',after)
anchor="gate=json.loads((old/'summary-6740.json').read_text()); assert gate['solver_records']==66"
assert code.count(anchor)==1
code=code.replace(anchor,"proof=root/'benchmarks/runs/20260909-6734-reduce-watermark-proof-control/summary.json'\nassert json.loads(proof.read_text())['checked_proofs']==8\nfor folder in ('provisional','results'):\n    for path in (old/folder).glob('*.json'):\n        state=classify(json.loads(path.read_text()),folder=='results')\n        assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(path,state)")
assert "(old/'summary-6740.json',witness)" in code
code=code.replace("(old/'summary-6740.json',witness)",'(proof,witness)')
code=code.replace("acceptance='All6","acceptance='Full B/C66 checked/clean remains required; these experimental waves may overlap it. All6")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
