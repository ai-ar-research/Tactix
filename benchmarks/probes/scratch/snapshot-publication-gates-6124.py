#!/usr/bin/env python3
"""Record completed solver phases separately from still-pending certificates."""
import csv
import datetime as dt
import json
import math
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import fixed_runtime as f
full=root/'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50'
bc=root/'benchmarks/runs/20260908-6114-substitute-publication-BC-gate'
target=root/'benchmarks/reports/2026-09-08-publication-gates-6124.json';assert not target.exists()
score=f.score(full);assert score['records']==score['expected_solves']==50
m=json.loads((bc/'manifest.json').read_text());records={};hashes={}
for rel,digest in m['pin_sha256'].items():assert f.sha(bc/rel)==digest,rel
for folder in ('provisional','results'):
    for p in (bc/folder).glob('*.json'):
        r=json.loads(p.read_text());task=r['task'];assert task==int(p.stem)
        assert r['expected_match'] and not r['solve']['timed_out']
        if folder=='results':
            assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
            assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
            for key in ('solve','certificate_sha256','certificate_bytes'):assert records[task][key]==r[key]
        records[task]=r;hashes[str(p.relative_to(root))]=f.sha(p)
assert set(records)==set(range(66))
groups={}
for tier,count in (('B',28),('C',5)):
    times={}
    for prefix in ('current','candidate'):
        rows=[r for r in records.values() if r['arm']==prefix+'_'+tier]
        assert len(rows)==count
        times[prefix]=math.fsum(r['solve']['wall_seconds'] for r in rows)
    groups[tier]=dict(times,ratio=times['candidate']/times['current'])
for name in ('manifest.json','matrix.tsv','array.sbatch','launch.json'):hashes[str((bc/name).relative_to(root))]=f.sha(bc/name)
for folder in ('provisional','results'):
    for p in (full/folder).glob('*.json'):hashes[str(p.relative_to(root))]=f.sha(p)
out=dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),full50=score,BC=groups,
         BC_checked=sum(bool(r.get('valid')) for r in records.values()),hashes=hashes,
         limitation='6107 solver snapshot,not promotion: pendingproofs/current-sourcefuzz anddiagnostic validation remain. Different windows do not prove a causal runtime gain.')
target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(dict(full_seconds=score['candidate_par2'],full_ratio=score['ratio'],
    full_checked=score['checked_clean'],BC=groups,BC_checked=out['BC_checked']),indent=2))
