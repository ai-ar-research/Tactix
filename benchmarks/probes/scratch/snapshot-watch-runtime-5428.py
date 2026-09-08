#!/usr/bin/env python3
"""Score all three exact-work pairs; pending checks never establish acceptance."""
import datetime
import hashlib
import json
from pathlib import Path
import statistics
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5428-watch-truncate-rank35-runtime'
m=json.loads((c/'manifest.json').read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for rel,digest in m['pin_sha256'].items():assert sha(c/rel)==digest
rows={};paths={}
for kind in ('provisional','results'):
    for p in (c/kind).glob('*.json'):
        r=json.loads(p.read_text())
        assert r['expected_match'] and r['solve']['exit_code']==20 and not r['solve']['timed_out']
        if kind=='results':
            assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
            assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
            assert r['proof_check']['exit_code']==0 and not r['proof_check']['timed_out']
        rows[r['task']]=r;paths[r['task']]=p
if len(rows)!=6:
    print('5428 solver results pending',len(rows),'of 6');raise SystemExit(0)
waves=[]
for wave in range(3):
    t,k=rows[2*wave],rows[2*wave+1]
    assert (t['arm'],k['arm'])==('candidate','previous')
    stats=lambda r:[s for s in r['solver_statistics'] if not s.startswith('c timing')]
    assert stats(t)==stats(k), 'Behavior-preserving premise failed: inspect exact statistics before interpreting timing'
    assert t['certificate_sha256']==k['certificate_sha256'], 'Exact proof identity premise failed'
    ts,ks=t['solve']['wall_seconds'],k['solve']['wall_seconds']
    waves.append(dict(wave=wave,candidate_seconds=ts,previous_seconds=ks,ratio=ts/ks,proof_sha256=t['certificate_sha256']))
ratios=[w['ratio'] for w in waves]
spread={}
for arm in ('candidate','previous'):
    times=[r['solve']['wall_seconds'] for r in rows.values() if r['arm']==arm]
    spread[arm]=dict(median=statistics.median(times),min=min(times),max=max(times),range_over_median=(max(times)-min(times))/statistics.median(times))
checked=sum(r.get('valid',False) for r in rows.values())
result=dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),checked_clean=checked,solver_results=6,
    same_non_timing_statistics_and_proof=True,waves=waves,paired=dict(median=statistics.median(ratios),min=min(ratios),max=max(ratios)),spread=spread,
    absolute_range_within_two_percent=all(r['range_over_median']<=.02 for r in spread.values()),
    records={str(i):dict(path=str(p.relative_to(root)),sha256=sha(p)) for i,p in paths.items()},
    limitation='Exact-work/proof identity supports a unit-cost comparison, not final correctness before checks or global competitiveness. Candidate remains UNFUZZED and unpromoted.')
p=c/('summary.json' if checked==6 else 'solver-checkpoint.json')
if p.exists():
    print('Frozen checkpoint exists:',p);raise SystemExit(0)
p.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ('checked_clean','paired','spread','absolute_range_within_two_percent')},indent=2))
