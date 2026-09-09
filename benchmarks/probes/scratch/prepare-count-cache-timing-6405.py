#!/usr/bin/env python3
"""Predeclare three exact-work cost waves against the cache predecessor."""
import json
from pathlib import Path
source=Path(__file__).with_name('prepare-collapse-timing-6332.py')
root=source.resolve().parents[3]
gate=root/'benchmarks/runs/20260909-6387-vivify-count-cache-BC-gate/solver-checkpoint-6392.json'
if not gate.exists():
    print('Complete exact-work B/C checkpoint pending; no campaign created')
    raise SystemExit(0)
g=json.loads(gate.read_text())
assert g['solver_records']==66 and g['exact_candidate_parent_work_and_proof']
code=source.read_text()
for old,new in {
    '20260909-6332-collapse-three-wave-runtime':'20260909-6405-count-cache-three-wave-runtime',
    '20260909-6311-substitute-collapse-BC-gate':'20260909-6387-vivify-count-cache-BC-gate',
    "('candidate',6305,'substitute-collapse','20260909-6320-substitute-collapse-ranks1-50')":"('candidate',6383,'vivify-count-cache','20260909-6366-compact-stamp-ranks1-50')",
    "('previous',6269,'vivify-binary-retry','20260909-6278-vivify-binary-ranks1-50')":"('previous',6357,'compact-stamp','20260909-6366-compact-stamp-ranks1-50')",
    '(32,33,40)':'(32,33,35)',
    '6332-collapse-timing':'6405-count-cache-timing',
    'tactix-collapse-changed-work-three-waves-v1':'tactix-count-cache-exact-work-three-waves-v1',
    "'32':'primary Rank32 search reduction','33':'second productive changed-work family','40':'observed adverse search-work control'":"'32':'profile-primary count-sort cost witness','33':'productive stamp-repair trajectory preservation','35':'adverse stamp-repair trajectory preservation'",
    '6305 retained-endpoint retry causes repeatable changed-work runtime effects versus6269, including the adverseRank40 case.':'6383 cached leading counts reduce Vivify schedule cost while preserving every6357 proof byte and non-timing statistic. No speedup assumed; all three pairs and each arm spread decide.',
    'Unchanged pinned runner/oracles; existing39 benchmark13watcher tests. Matrix validates18tasks,pairedarmcoverage; source gates105/104,72fixtures,8proofs alreadypassed. New timing compares different search work,not isolated instruction cost.':'Unchanged runner/oracles and63benchmark13watcher tests;107Release twice106trace,576actualold/new fixtures and48independent proofs. Both arms bind the identical6357 work/proof oracle at each rank;6387 all66 solver outcomes and33candidate controls required. New matrix explicitly checks paired coverage.',
}.items():
    assert old in code,old
    code=code.replace(old,new)
marker="m['pin_sha256']="
assert code.count(marker)==1
code=code.replace(marker,"m['provisional_gate']={'path':str(checkpoint.relative_to(root)),'sha256':sha(checkpoint)}\nassert oracles['candidate']==oracles['previous']\n"+marker)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__','checkpoint':gate})
