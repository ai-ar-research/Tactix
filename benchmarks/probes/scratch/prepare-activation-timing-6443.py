#!/usr/bin/env python3
"""Three predeclared paired waves once current full-run witness oracles exist."""
from pathlib import Path
source=Path(__file__).with_name('prepare-collapse-timing-6332.py')
root=source.resolve().parents[3]
run=root/'benchmarks/runs/20260909-6439-activation-ranks1-50'
for rank in (32,35,45):
    name=f'{rank-1:03}.json'
    if not (run/'results'/name).exists() and not (run/'provisional'/name).exists():
        print('Current32/35/45 full-run work/proof oracles pending; no campaign created'); raise SystemExit(0)
code=source.read_text()
for old,new in {
 '20260909-6332-collapse-three-wave-runtime':'20260909-6443-activation-three-wave-runtime',
 '20260909-6311-substitute-collapse-BC-gate':'20260909-6427-activation-stamp-BC-gate',
 "('candidate',6305,'substitute-collapse','20260909-6320-substitute-collapse-ranks1-50')":"('candidate',6424,'activation-stamp','20260909-6439-activation-ranks1-50')",
 "('previous',6269,'vivify-binary-retry','20260909-6278-vivify-binary-ranks1-50')":"('previous',6383,'vivify-count-cache','20260909-6407-count-cache-ranks1-50')",
 '(32,33,40)':'(32,35,45)',
 '6332-collapse-timing':'6443-activation-timing',
 'tactix-collapse-changed-work-three-waves-v1':'tactix-activation-changed-work-three-waves-v1',
 "'32':'primary Rank32 search reduction','33':'second productive changed-work family','40':'observed adverse search-work control'":"'32':'preserved3M closure and remaining unit-cost gap','35':'large adverse stamp-repair search-work control','45':'causal first-activation repair witness with restored100K closure'",
 '6305 retained-endpoint retry causes repeatable changed-work runtime effects versus6269, including the adverseRank40 case.':'6424 native first-activation stamps produce repeatable runtime effects versus6383. Include the primary45 repair, preserved32 closure and adverse35 control; no speedup assumed.',
 'Unchanged pinned runner/oracles; existing39 benchmark13watcher tests. Matrix validates18tasks,pairedarmcoverage; source gates105/104,72fixtures,8proofs alreadypassed. New timing compares different search work,not isolated instruction cost.':'Unchanged runner and maintained65benchmark13watcher tests;108Release twice107trace and80actualparentfailures repaired with20nativecontrols. All33BC expected and Rank45/32 bounded closures are prerequisites to the full-run oracles. Matrix checks all18 paired tasks; exact own work/proof required. Changed-work timing, not isolated instruction cost.',
}.items():
    assert old in code,old; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
