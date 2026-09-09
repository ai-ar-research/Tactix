#!/usr/bin/env python3
"""Freeze the current6049 full50 starting score, preserving pending checks."""
import datetime as dt
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import fixed_runtime as f
c=root/'benchmarks/runs/20260908-6108-rank41-50-matched'
old=root/'benchmarks/runs/20260908-6061-root-lock-ranks1-40'
ref=f.reference()
m=json.loads((c/'manifest.json').read_text())
om=json.loads((old/'manifest.json').read_text())
assert m['arms']['tactix']['sha256']==om['arms']['tactix']['sha256']
hashes={}
def bind(p):hashes[str(p.relative_to(root))]=f.sha(p)
def records(campaign):
    manifest=json.loads((campaign/'manifest.json').read_text())
    for name in ('manifest.json','matrix.tsv','array.sbatch','launch.json'):bind(campaign/name)
    for rel,digest in manifest['pin_sha256'].items():assert f.sha(campaign/rel)==digest,rel
    out={}
    for folder in ('provisional','results'):
        for p in sorted((campaign/folder).glob('*.json')):
            r=json.loads(p.read_text());bind(p)
            assert int(p.stem)==r['task']
            assert r['expected_match'] and not r['solve']['timed_out']
            if folder=='results':
                assert r['valid'] and r['binary_sha256']==manifest['arms'][r['arm']]['sha256']
                assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
                for key in ('solve','certificate_sha256','certificate_bytes','instance','rank','arm'):
                    assert out[r['task']][key]==r[key],key
            out[r['task']]=r
    return out
prior=records(old);extension=records(c)
assert len(prior)==40 and len(extension)==20
for task,r in prior.items():assert r['rank']==task+1 and r['arm']=='tactix'
for task,r in extension.items():
    assert r['rank']==41+task//2 and r['arm']==('tactix','kissat')[task%2]
candidate=list(prior.values())+[extension[t] for t in range(0,20,2)]
summary=f.score_records(ref,candidate)
assert summary['complete'] and summary['expected_solves']==50
summary.update(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    candidate_source=om['candidate_authority'],candidate_sha256=m['arms']['tactix']['sha256'],
    reference=dict(path=str(f.REFERENCE.relative_to(root)),sha256=f.sha(f.REFERENCE)),
    starting_candidate='6049, not the newly submitted6107 repair',
    extension_checked=sum(bool(r.get('valid')) for r in extension.values()),
    extension=[dict(rank=41+i,tactix=extension[2*i]['solve']['wall_seconds'],
        kissat=extension[2*i+1]['solve']['wall_seconds'],
        ratio=extension[2*i]['solve']['wall_seconds']/extension[2*i+1]['solve']['wall_seconds'],
        verdict=extension[2*i]['expected']) for i in range(10)],
    hashes=hashes,limitation='Original40+new10 use identical6049 binary and matched resource contract,but different time windows. Pending certificates are not verified. No repair speedup or unit-cost attribution.')
target=root/'benchmarks/reports/2026-09-08-rank50-start.json';assert not target.exists()
target.write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:summary[k] for k in ('candidate_par2','fixed_kissat_seconds','ratio','checked_clean','extension_checked','extension')},indent=2))
