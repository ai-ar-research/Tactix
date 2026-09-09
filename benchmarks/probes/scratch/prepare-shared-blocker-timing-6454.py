#!/usr/bin/env python3
"""Predeclare three exact-work propagation-cost waves after the complete BC gate."""
import json
from pathlib import Path
source=Path(__file__).with_name('prepare-collapse-timing-6332.py')
root=source.resolve().parents[3]
gate=root/'benchmarks/runs/20260909-6451-shared-blocker-BC-gate/solver-checkpoint-6453.json'
full=root/'benchmarks/runs/20260909-6439-activation-ranks1-50'
if not gate.exists():
    print('Complete exact-work6453 B/C gate pending; no output created'); raise SystemExit(0)
g=json.loads(gate.read_text())
assert g['solver_records']==66 and g['exact_parent_candidate_work_and_proof']
for rank in (32,35,45):
    name=f'{rank-1:03}.json'
    if not (full/'results'/name).exists() and not (full/'provisional'/name).exists():
        print('Parent full-run32/35/45 oracles pending; no output created'); raise SystemExit(0)
code=source.read_text()
for old,new in {
 '20260909-6332-collapse-three-wave-runtime':'20260909-6454-shared-blocker-three-wave-runtime',
 '20260909-6311-substitute-collapse-BC-gate':'20260909-6451-shared-blocker-BC-gate',
 "('candidate',6305,'substitute-collapse','20260909-6320-substitute-collapse-ranks1-50')":"('candidate',6448,'shared-blocker','20260909-6439-activation-ranks1-50')",
 "('previous',6269,'vivify-binary-retry','20260909-6278-vivify-binary-ranks1-50')":"('previous',6424,'activation-stamp','20260909-6439-activation-ranks1-50')",
 '(32,33,40)':'(32,35,45)',
 '6332-collapse-timing':'6454-shared-blocker-timing',
 'tactix-collapse-changed-work-three-waves-v1':'tactix-shared-blocker-exact-work-three-waves-v1',
 "'32':'primary Rank32 search reduction','33':'second productive changed-work family','40':'observed adverse search-work control'":"'32':'profile-primary propagation execution-cost witness','35':'adverse search-work preservation control','45':'activation-repair trajectory preservation control'",
 '6305 retained-endpoint retry causes repeatable changed-work runtime effects versus6269, including the adverseRank40 case.':'Sharing the satisfied-blocker exit reduces propagation execution cost without changing any6424 work or certificate. No speedup assumed. Reject a neutral/noisy experiment as a convergence improvement; no extra selective waves.',
 'Unchanged pinned runner/oracles; existing39 benchmark13watcher tests. Matrix validates18tasks,pairedarmcoverage; source gates105/104,72fixtures,8proofs alreadypassed. New timing compares different search work,not isolated instruction cost.':'Reused maintained65benchmark13watcher tests and unchanged runner. Source109Release twice108trace pass; actual old/new180mixed-watch and224cursor fixtures have exact digests. All66BC expected and33candidate work/proof identities prerequisite. Both timing arms bind the same parent oracle; all18 tasks checked.',
}.items():
    assert old in code,old; code=code.replace(old,new)
marker="m['pin_sha256']="; assert code.count(marker)==1
code=code.replace(marker,"m['provisional_gate']={'path':str(checkpoint.relative_to(root)),'sha256':sha(checkpoint)}\nassert oracles['candidate']==oracles['previous']\n"+marker)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__','checkpoint':gate})
