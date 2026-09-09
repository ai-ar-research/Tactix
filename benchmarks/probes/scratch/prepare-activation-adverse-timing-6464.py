#!/usr/bin/env python3
"""Price the three changed-work losses selected from the complete50 snapshot."""
import json
from pathlib import Path
source=Path(__file__).with_name('prepare-collapse-timing-6332.py')
root=source.resolve().parents[3]
full=root/'benchmarks/reports/2026-09-09-activation-full50-6441/summary.json'
summary=json.loads(full.read_text()); assert len(summary['rows'])==50
losses=sorted((r for r in summary['rows'] if not r['certificate_exact_parent']),
              key=lambda r:r['delta_parent_seconds'],reverse=True)
assert [r['rank'] for r in losses[:3]]==[33,17,43]
code=source.read_text()
for old,new in {
 '20260909-6332-collapse-three-wave-runtime':'20260909-6464-activation-adverse-three-wave-runtime',
 '20260909-6311-substitute-collapse-BC-gate':'20260909-6427-activation-stamp-BC-gate',
 "('candidate',6305,'substitute-collapse','20260909-6320-substitute-collapse-ranks1-50')":"('candidate',6424,'activation-stamp','20260909-6439-activation-ranks1-50')",
 "('previous',6269,'vivify-binary-retry','20260909-6278-vivify-binary-ranks1-50')":"('previous',6383,'vivify-count-cache','20260909-6407-count-cache-ranks1-50')",
 '(32,33,40)':'(17,33,43)',
 "('pending_validation','checked_UNSAT')":"('pending_validation','checked_UNSAT','checked_SAT')",
 "r['expected']=='UNSAT'":"r['expected'] in ('UNSAT','SAT')",
 '6332-collapse-timing':'6464-activation-adverse-timing',
 'tactix-collapse-changed-work-three-waves-v1':'tactix-activation-adverse-three-waves-v1',
 "'32':'primary Rank32 search reduction','33':'second productive changed-work family','40':'observed adverse search-work control'":"'17':'second-largest changed-work parent loss, SAT still faster than fixed Kissat','33':'largest changed-work parent loss, UNSAT now slower than fixed Kissat','43':'third-largest changed-work parent loss, UNSAT added-rank gap'",
 '6305 retained-endpoint retry causes repeatable changed-work runtime effects versus6269, including the adverseRank40 case.':'The complete6441 table identifies33,17,43 as the three largest changed-certificate parent runtime losses. Measure all three independently from the already-frozen32/35/45 repair-witness campaign; no replacement, selective extra repeat or score exclusion.',
 'Unchanged pinned runner/oracles; existing39 benchmark13watcher tests. Matrix validates18tasks,pairedarmcoverage; source gates105/104,72fixtures,8proofs alreadypassed. New timing compares different search work,not isolated instruction cost.':'Reuse existing65benchmark13watcher and checked SAT/UNSAT classification tests. No runner/parser/checker edit; composition accepts both expected statuses because17 is SAT. All18 matrix/input/expected identities and own-arm exact work/certificate required. Full50 snapshot determines the top-three changed-certificate losses before submission.',
}.items():
    assert old in code,old; code=code.replace(old,new)
anchor="m['pin_sha256']="; assert code.count(anchor)==1
code=code.replace(anchor,"m['complete50_selection']={'path':str(selection_evidence.relative_to(root)),'sha256':sha(selection_evidence),'rule':'three largest parent runtime losses among changed-certificate ranks; all50 retained'}\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__','selection_evidence':full})
