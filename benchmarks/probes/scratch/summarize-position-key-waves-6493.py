#!/usr/bin/env python3
"""All three exact-work waves, with separately sampled RSS and VmPeak."""
from pathlib import Path
source=Path(__file__).with_name('summarize-collapse-waves-6343.py')
code=source.read_text()
for old,new in {
 '20260909-6332-collapse-three-wave-runtime':'20260909-6491-position-key-three-wave-runtime',
 'solver-checkpoint-6343.json':'solver-checkpoint-6493.json',
 '(32, 33, 40)':'(10, 25, 47)',
 "('pending_validation', 'checked_UNSAT')":"('pending_validation', 'checked_UNSAT', 'checked_SAT')",
 'Changed-work candidate6305 versus6269, not isolated unit cost or fixed50 competitiveness.':'Exact-work position-key6475 versus6424 on predeclared memory witnesses. RSS is solver peak resident memory; sampled VmPeak is virtual address space and may miss short peaks. No fixed50 competitiveness or hard16GiB claim.',
}.items():
    assert old in code,old; code=code.replace(old,new)
anchor='target.write_text('; assert code.count(anchor)==1
extra="""from paired_memory import summarize_memory
result['memory']={rank:summarize_memory(dict(wave=t//6,arm=r['arm'],solve=r['solve'])
    for t,r in rows.items() if r['rank']==rank) for rank in (10,25,47)}
assert all(v['peak_rss_kb']['complete'] for v in result['memory'].values())
"""
code=code.replace(anchor,extra+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
