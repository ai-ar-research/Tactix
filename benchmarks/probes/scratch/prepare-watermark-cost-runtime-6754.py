#!/usr/bin/env python3
"""Predeclare matched Rank38 pairs for the adverse unchanged-work observation."""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),'--deadline','2026-09-10T06:00:00-04:00'],check=True)
source=Path(__file__).with_name('prepare-material-runtime-6592.py')
code=source.read_text()
changes={
 '20260909-6592-rank50-material-three-wave-runtime':'20260909-6754-rank38-watermark-cost-runtime',
 '20260909-6584-material-schedule-BC-gate':'20260909-6737-reduce-watermark-BC-gate',
 'summary-6586.json':'summary-6740.json',
 'benchmarks/runs/20260909-6587-rank50-material-schedule/summary-6589.json':'benchmarks/reports/2026-09-09-reduce-watermark-full50-6750/summary.json',
 "w['observer_neutral'] and w['decision_difference']['complete_equal']":"w['score']['expected_solves']==50 and w['rows'][37]['rank']==38 and w['rows'][37]['non_timing_stats_exact_parent'] and w['rows'][37]['certificate_exact_parent']",
 '20260909-6439-activation-ranks1-50':'20260909-6666-congruence-empty-ranks1-50',
 'results/049.json':'results/037.json',"r['rank']==50":"r['rank']==38",
 "r['expected']=='SAT'":"r['expected']=='UNSAT'",
 "('candidate','6579-material-schedule'),('previous','6424-activation-stamp')":"('candidate','6730-reduce-watermark'),('previous','6654-congruence-empty-gates')",
 "rank=50,tier='D',instance=r['instance'],expected='SAT'":"rank=38,tier='D',instance=r['instance'],expected='UNSAT'",
 "'6584-material-schedule-BC','6592-rank50-material-runtime'":"'6737-reduce-watermark-BC','6754-rank38-watermark-cost'",
 'Price the successful general Rank50 material scheduling repair against its immutable6424 control.':'Determine whether the Rank38 unchanged-work slowdown from the watermark repair repeats under matched timing; retain every adverse outcome.',
 'Restoring the823562 material invitation reduces full Rank50 search work and runtime. No absolute gain assumed from prefix equality.':'Full50 shows Rank38+6.17% with exact parent work/certificate, while49 unchanged-work ranks collectively gain123.911s. Test repeated current/parent cost before attributing layout or instruction overhead rather than cross-window variation.',
 'previous arm exact to6439':'both arms exact to6666'}
for i,(before,after) in enumerate(changes.items()):
    assert before in code,before;code=code.replace(before,f'__SUB_{i}__')
for i,after in enumerate(changes.values()):code=code.replace(f'__SUB_{i}__',after)
code=code.replace("acceptance='All6","acceptance='Unchanged work requires candidate task0 also exact to previous_oracle; no source edit selected without repeated evidence. All6")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
