#!/usr/bin/env python3
"""Reused B/C scoring plus complete parent6357 work/proof neutrality."""
from pathlib import Path
source=Path(__file__).with_name('check-compact-stamp-gate-6364.py')
code=source.read_text()
for old,new in {
    '20260909-6362-compact-stamp-BC-gate':'20260909-6387-vivify-count-cache-BC-gate',
    'solver-checkpoint-6364.json':'solver-checkpoint-6392.json',
}.items():
    assert old in code
    code=code.replace(old,new)
extra='''
parent=root/'benchmarks/runs/20260909-6362-compact-stamp-BC-gate'
oracles={}
for task,(_,r,_) in latest.items():
    if not r['arm'].startswith('candidate'): continue
    p=parent/'results'/f'{task:03}.json'
    if not p.exists(): p=parent/'provisional'/p.name
    old=json.loads(p.read_text())
    assert classify(old,p.parent.name=='results')['status'] in ('pending_validation','checked_SAT','checked_UNSAT')
    for key in ('arm','instance','expected','verdict','certificate_sha256','certificate_bytes'):
        assert r[key]==old[key],(task,key)
    nt=lambda value:[s for s in value['solver_statistics'] if not s.startswith('c timing')]
    assert nt(r)==nt(old),task
    oracles[str(p.relative_to(root))]=sha(p)
assert len(oracles)==33
'''
assert code.count('by_tier = {}')==1
code=code.replace('by_tier = {}',extra+'\nby_tier = {}')
code=code.replace('result = dict(solver_records=66,','result = dict(exact_candidate_parent_work_and_proof=True, parent_oracles=oracles, solver_records=66,')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
