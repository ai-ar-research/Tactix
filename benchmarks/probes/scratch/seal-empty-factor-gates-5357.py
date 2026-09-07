#!/usr/bin/env python3
"""Validate and seal the checked, fixed-membership empty-Factor screens."""
import collections
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
for name,count in [('20260907-5356-empty-factor-runtime',12),('20260907-5357-empty-factor-BC-gate',66)]:
    c=root/'benchmarks/runs'/name;m=json.loads((c/'manifest.json').read_text())
    files=sorted((c/'results').glob('*.json'))
    if len(files)!=count:
        print(name,'pending',len(files),'of',count);continue
    for rel,sha in m['pin_sha256'].items():assert hashlib.sha256((c/rel).read_bytes()).hexdigest()==sha,rel
    rs=[json.loads(p.read_text()) for p in files]
    groups=collections.defaultdict(list)
    for r in rs:
        assert r['valid'] and r['expected_match'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
        assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
        assert not r['solve']['timed_out'] and r['solve']['exit_code'] in (10,20)
        check=r['model_check'] if r['solve']['exit_code']==10 else r['proof_check']
        assert check['exit_code']==0 and not check['timed_out']
        groups[r['arm']].append(r)
    totals={arm:dict(solved=len(rows),sat=sum(r['solve']['exit_code']==10 for r in rows),unsat=sum(r['solve']['exit_code']==20 for r in rows),seconds=round(sum(r['solve']['wall_seconds'] for r in rows),3),max_rss_kb=max(r['solve']['peak_rss_kb'] for r in rows)) for arm,rows in groups.items()}
    by_instance=collections.defaultdict(dict)
    for r in rs:by_instance[r['instance']][r['arm']]=r['solve']['wall_seconds']
    if count==12:
        ratios={a+'_over_current':t['seconds']/totals['current']['seconds'] for a,t in totals.items() if a!='current'}
        ratios['candidate_over_previous']=totals['candidate']['seconds']/totals['previous']['seconds']
        ratios['candidate_over_kissat']=totals['candidate']['seconds']/totals['kissat']['seconds']
    else:
        assert all(len(v)==2 for v in by_instance.values()) and len(by_instance)==33
        ratios={tier:totals['candidate_'+tier]['seconds']/totals['current_'+tier]['seconds'] for tier in ('B','C')}
        ratios['combined']=sum(totals['candidate_'+t]['seconds'] for t in ('B','C'))/sum(totals['current_'+t]['seconds'] for t in ('B','C'))
    result=dict(checked_clean=count,timeouts=0,arms=totals,ratios=ratios,per_instance_seconds=dict(sorted(by_instance.items())),result_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in files},limitations='One diagnostic wave; isolated cumulative candidate UNFUZZED. Not a Rank 1–40 acceptance or promotion.')
    dest=c/'summary.json'
    if dest.exists():assert json.loads(dest.read_text())==result
    else:dest.write_text(json.dumps(result,indent=2)+'\n')
    print(name,json.dumps({k:v for k,v in result.items() if k not in ('result_sha256','per_instance_seconds')}))
