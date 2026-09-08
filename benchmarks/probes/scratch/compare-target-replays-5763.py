#!/usr/bin/env python3
"""Validate complete candidate replay and immutable cached native controls."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(8 << 20),b''):h.update(block)
    return h.hexdigest()
for name in ('20260908-5755-reuse-target-rank31-replay','20260908-5756-reuse-target-rank33-replay'):
    c=root / 'benchmarks/runs' / name
    if (c / 'summary.json').exists():
        print('Already sealed',name)
        continue
    result=c / 'results/candidate.json'
    if not result.exists():
        print('Pending',name)
        continue
    m=json.loads((c / 'manifest.json').read_text())
    r=json.loads(result.read_text())
    assert r['exit_code']==0 and r['scratch_deleted']
    assert r['binary_sha256']==sha(c / 'pins/candidate')
    for rel,digest in m['hashes'].items():assert sha(c / rel)==digest
    parent=root / m['parent_campaign']
    pm=json.loads((parent / 'manifest.json').read_text())
    for key in ('input','input_sha256_compressed','stop_conflict','stream_from','common_decisions'):
        assert m[key]==pm[key],key
    controls={}
    for arm,binding in m['cached_controls'].items():
        assert sha(root / binding['record'])==binding['sha256']
        control=json.loads((root / binding['record']).read_text())
        assert control['exit_code']==0 and control['scratch_deleted']
        assert control['binary_sha256']==sha(parent / 'pins' / arm)
        for rel,digest in control['trace_sha256'].items():assert sha(parent / 'traces' / rel)==digest
        controls[arm]=control
    for rel,digest in r['trace_sha256'].items():assert sha(c / 'traces' / rel)==digest
    assert r['trace_sha256']==controls['candidate']['trace_sha256'], 'DefaultPolicy trace changed'
    assert r['partial_certificate']==controls['candidate']['partial_certificate'], 'DefaultPolicy proof changed'
    output=dict(candidate_parent_trace_and_proof_equal=True,cached_native_validated=True,
        stop_conflict=m['stop_conflict'],stream_from=m['stream_from'],
        trace_sha256=r['trace_sha256'],partial_certificate=r['partial_certificate'],
        manifest_sha256=sha(c / 'manifest.json'),result_sha256=sha(result),
        limitation='Stopped observer equality,not acceptance timing or final proof. Native not rerun.')
    (c / 'summary.json').write_text(json.dumps(output,indent=2)+'\n')
    print('PASS complete parent replay and cached native:',name,flush=True)
