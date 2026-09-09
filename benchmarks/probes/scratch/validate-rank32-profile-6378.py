#!/usr/bin/env python3
"""Validate sampled work/proof replay as soon as both solvers publish results."""
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root/'benchmarks/runs/20260909-6372-rank32-release-profile'
if any(not (c/'results'/f'{i:03}.json').exists() and
       not (c/'provisional'/f'{i:03}.json').exists() for i in range(2)):
    print('Profile solver results still pending; no summary created')
    raise SystemExit(0)
source = Path(__file__).with_name('seal-rank4-profile-6168.py')
code = source.read_text()
for old,new in (
    ('20260909-6166-rank4-release-profile','20260909-6372-rank32-release-profile'),
    ("'summary.json'", "'solver-summary-6378.json'"),
    ("    r = json.loads(path.read_text())", "    if not path.exists(): path=c/f'provisional/{task:03d}.json'\n    r = json.loads(path.read_text())"),
    ("    assert classify(r, True)['checked'] and classify(old, True)['checked']",
     "    state=classify(r,path.parent.name=='results')\n    assert state['status'] in ('pending_validation','checked_UNSAT')\n    assert classify(old,old_path.parent.name=='results')['status'] in ('pending_validation','checked_UNSAT')"),
    ("'instance', 'expected', 'verdict', 'binary_sha256',", "'instance', 'expected', 'verdict',"),
    ("    assert r['binary_sha256'] == m['arms'][arm]['sha256']",
     "    if path.parent.name=='results': assert r['binary_sha256']==m['arms'][arm]['sha256']"),
    ('checked_clean=True, work=work', "checked_clean=state['checked'], work=work"),
    ('checked_clean=2, arms=arms', "checked_clean=sum(a['checked_clean'] for a in arms.values()), arms=arms"),
    ('Rank4 current Release profiles are neutral by exact within-arm work/proof replay. Learned-level radix sorting is a bounded engineering target; no scheduling change or runtime gain is established.',
     'Rank32 current Release profiles exactly replay each unprofiled arm in proof bytes/hash and recorded work. Candidate non-timing statistics are exact. Use self/callpath samples to select a general engineering target, not a scheduling or runtime-gain claim.'),
    ('One approximately eight-second sampled pair, not repeated timing acceptance. Cross-solver function/timer and propagation scopes differ; compare neither raw percentages nor propagation counts as equal operations.',
     'One sampled pair, not repeated timing acceptance. Pending final certificates remain unverified. Cross-solver function/timer and propagation scopes differ; neither raw percentages nor propagation counts are equal-operation prices.'),
):
    assert old in code, old
    code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
