#!/usr/bin/env python3
"""Record the already-submitted first5551 pair and its completed B28 precondition."""
from pathlib import Path
import datetime
import hashlib
import json
root = Path(__file__).resolve().parents[3]
c = root/'benchmarks/runs/20260908-5551-vivify-census-rank39-runtime'
gate = root/'benchmarks/runs/20260908-5550-vivify-census-BC-gate'
p = c/'wave0-submission.json'
assert not p.exists()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((c/'manifest.json').read_text())
for name,digest in m['pin_sha256'].items(): assert sha(c/name)==digest
records={}
for task in range(56):
    path=gate/'results'/f'{task:03d}.json'
    r=json.loads(path.read_text())
    assert r['task']==task and r['expected_match'] and r['valid']
    assert r['solve']['exit_code'] in (10,20) and not r['solve']['timed_out']
    assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    checker=r['model_check'] if r['solve']['exit_code']==10 else r['proof_check']
    assert checker['exit_code']==0 and not checker['timed_out']
    records[path.name]=sha(path)
p.write_text(json.dumps(dict(job='1672509',tasks=[0,1],
    command='sbatch --parsable --array=0-1 benchmarks/runs/20260908-5551-vivify-census-rank39-runtime/array.sbatch',
    recorded_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    manifest_sha256=sha(c/'manifest.json'),gate_manifest_sha256=sha(gate/'manifest.json'),
    B28_checked_results=records,
    continuation='After both wave0 expected solver snapshots, submit2-3; after wave1 solver snapshots, submit4-5. Proofs remain asynchronous. Candidate5546 is not covered by parent5522 fuzz5539; no promotion.'),indent=2)+'\n')
print(p)
