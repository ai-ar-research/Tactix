#!/usr/bin/env python3
"""Predeclare the sole changed-work adverse rank from the complete50 table."""
import json
from pathlib import Path
source=Path(__file__).with_name('prepare-collapse-timing-6332.py')
root=source.resolve().parents[3]
selection=root/'benchmarks/reports/2026-09-09-material-full50-6596/summary.json'
full=json.loads(selection.read_text()); assert len(full['rows'])==50
adverse=[r for r in full['rows'] if not r['certificate_exact_parent'] and r['delta_parent_seconds']>0]
assert [r['rank'] for r in adverse]==[38]
code=source.read_text()
for old,new in {
 '20260909-6332-collapse-three-wave-runtime':'20260909-6599-material-adverse-rank38',
 '20260909-6311-substitute-collapse-BC-gate':'20260909-6584-material-schedule-BC-gate',
 "('candidate',6305,'substitute-collapse','20260909-6320-substitute-collapse-ranks1-50')":"('candidate',6579,'material-schedule','20260909-6594-material-ranks1-50')",
 "('previous',6269,'vivify-binary-retry','20260909-6278-vivify-binary-ranks1-50')":"('previous',6424,'activation-stamp','20260909-6439-activation-ranks1-50')",
 '(32,33,40)':'(38,)',
 'len(rows)==18':'len(rows)==6',
 'range(0,18,2)':'range(0,6,2)',
 "'--array=0-17'":"'--array=0-5'",
 "array='0-17'":"array='0-5'",
 '6332-collapse-timing':'6599-material-adverse-rank38',
 'tactix-collapse-changed-work-three-waves-v1':'tactix-material-adverse-three-waves-v1',
 "'32':'primary Rank32 search reduction','33':'second productive changed-work family','40':'observed adverse search-work control'":"'38':'sole changed-certificate parent loss in complete6596; now matches native conflicts/decisions, still faster than fixed native in one full run'",
 'Three sequential full-width six-task waves0-5,6-11,12-17':'Three sequential full-width two-task waves0-1,2-3,4-5',
 'All18 expected':'All6 expected',
 '6305 retained-endpoint retry causes repeatable changed-work runtime effects versus6269, including the adverseRank40 case.':'Measure the sole changed-work adverse rank38 from complete6596 without excluding it or substituting it for the already-declared Rank50 waves. Do not assume an absolute loss from a single cross-window result.',
 'Unchanged pinned runner/oracles; existing39 benchmark13watcher tests. Matrix validates18tasks,pairedarmcoverage; source gates105/104,72fixtures,8proofs alreadypassed. New timing compares different search work,not isolated instruction cost.':'Unchanged pinned runner/oracles and maintained149benchmark13watcher tests; actual material consumer110/110/109 suites and24 fixed proof/model controls. Six-task paired matrix, own-arm exact work/proof, full-table adverse selection. No source/checker change.',
}.items():
    assert old in code,old
    code=code.replace(old,new)
anchor="m['pin_sha256']="; assert code.count(anchor)==1
code=code.replace(anchor,"m['complete50_selection']={'path':str(selection.relative_to(root)),'sha256':sha(selection),'rule':'every changed-certificate rank with positive parent delta; exactly rank38'}\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__','selection':selection})
