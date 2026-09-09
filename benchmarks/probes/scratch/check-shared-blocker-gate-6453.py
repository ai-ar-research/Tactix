#!/usr/bin/env python3
"""All66 expected outcomes plus exact candidate6424 work/proof replay."""
from pathlib import Path
source=Path(__file__).with_name('check-compact-stamp-gate-6364.py')
code=source.read_text()
for old,new in {
 '20260909-6362-compact-stamp-BC-gate':'20260909-6451-shared-blocker-BC-gate',
 'solver-checkpoint-6364.json':'solver-checkpoint-6453.json',
}.items():
    assert old in code; code=code.replace(old,new)
anchor='by_tier = {}'; assert code.count(anchor)==1
extra="""parent=root/'benchmarks/runs/20260909-6427-activation-stamp-BC-gate'
oracles={}
for task,(_,r,_) in latest.items():
    if not r['arm'].startswith('candidate'): continue
    p=parent/'results'/f'{task:03}.json'
    if not p.exists(): p=parent/'provisional'/p.name
    old=json.loads(p.read_text())
    assert classify(old,p.parent.name=='results')['status'] in ('pending_validation','checked_SAT','checked_UNSAT')
    for key in ('instance','arm','expected','verdict','certificate_bytes','certificate_sha256'): assert r[key]==old[key],(task,key)
    nt=lambda v:[line for line in v['solver_statistics'] if not line.startswith('c timing')]
    assert nt(r)==nt(old),task
    oracles[str(p.relative_to(root))]=sha(p)
assert len(oracles)==33
"""
code=code.replace(anchor,extra+anchor)
anchor='target.write_text('; assert code.count(anchor)==1
code=code.replace(anchor,"result['parent_oracles']=oracles\nresult['exact_parent_candidate_work_and_proof']=True\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
