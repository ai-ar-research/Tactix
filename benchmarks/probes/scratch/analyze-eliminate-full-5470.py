#!/usr/bin/env python3
"""Use complete solver views to select the next loss, without cross-wave claims."""
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
runs=root/'benchmarks/runs'
old=runs/'20260907-5434-inprocess-root-ranks1-40/solver-checkpoint.json'
new=runs/'20260907-5454-eliminate-limit-ranks1-40/solver-checkpoint.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
views=[json.loads(p.read_text()) for p in (old,new)]
for v in views:
    assert v['solver_records']==80 and set(v['per_rank'])==set(map(str,range(1,41)))
    for pair in v['per_rank'].values():
        for r in pair.values():assert sha(root/r['record'])==r['record_sha256']
rows=[]
for rank,pair in views[1]['per_rank'].items():
    previous=views[0]['per_rank'][rank]
    t,k=pair['tactix'],pair['kissat']
    oldt=previous['tactix']
    row=dict(rank=int(rank),candidate_seconds=t['seconds'],kissat_seconds=k['seconds'],
        seconds_over_kissat=t['seconds']-k['seconds'],time_ratio=t['seconds']/k['seconds'],
        work=t['work'],kissat_work=k['work'],prior_candidate_work=oldt['work'],
        work_unchanged_from_prior=t['work']==oldt['work'],
        candidate_peak_rss_kb=t['peak_rss_kb'],kissat_peak_rss_kb=k['peak_rss_kb'],
        verification={a:r['verification'] for a,r in pair.items()})
    row['instance']=json.loads((root/t['record']).read_text())['instance']
    rows.append(row)
rows.sort(key=lambda r:-r['seconds_over_kissat'])
assert rows[0]['rank']==31
out=dict(source_sha256={str(p.relative_to(root)):sha(p) for p in (old,new)},
    full_view_ratio=views[1]['provisional_all_solved_time_ratio'],
    checked_at_solver_snapshot=views[1]['checked_clean'],ranked_losses=rows,
    decision='Rank 31 is now the largest measured positive delta. Rank 37 retains priority for the existing bound causal replay; add a complete Rank-31 prefix comparison on the tested bound candidate to locate an independent residual mechanism.',
    limitations='Campaign windows differ. These complete views select diagnostics, not a paired attribution of gain to any individual repair. Conflicts/propagations and peak RSS are not scope-matched unit costs or memory ownership.')
p=new.parent/'next-losses.json';assert not p.exists();p.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({k:v for k,v in out.items() if k!='ranked_losses'},indent=2))
print(json.dumps(rows[:3],indent=2))
