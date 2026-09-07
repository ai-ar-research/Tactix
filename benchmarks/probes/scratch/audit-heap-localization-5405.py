#!/usr/bin/env python3
"""Freeze scoped non-fuzz provenance and cleanup evidence at this checkpoint."""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
jobs = {5383: 1669005, 5389: 1669095, 5393: 1669174, 5395: 1669252,
        5396: 1669333, 5397: 1669336, 5398: 1669338, 5399: None,
        5400: 1669345, 5401: 1669397, 5402: None, 5403: 1669443,
        5404: 1669485, 5405: int(sys.argv[1])}
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()
campaigns = []
for n, job in jobs.items():
    c = next((root / 'benchmarks/runs').glob(f'20260907-{n}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, expected in m.get('pin_sha256', m.get('hashes', {})).items():
        assert sha(c / rel) == expected, (n, rel)
    for rel, expected in m.get('binaries', {}).items():
        assert sha(root / rel) == expected, (n, rel)
    for item in m.get('sources', {}).values():
        assert sha(root / item['source'] / item['file']) == item['sha256']
    records = sorted((c / 'results').glob('*.json'))
    if n >= 5396:
        for p in records:
            r = json.loads(p.read_text())
            assert r['scratch_deleted'] and r['exit_code'] == 0
    residue = [str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and p.suffix in ('.cnf', '.drat', '.lrat', '.grat', '.frat')]
    assert not residue, residue
    state = subprocess.run(['sacct', '-j', str(job), '-X', '--format=JobID,State,ExitCode', '--parsable2'], capture_output=True, text=True, check=True).stdout.splitlines() if job else None
    artifacts = [p for folder in ('pins', 'traces') for p in (c / folder).glob('*') if p.is_file()]
    campaigns.append(dict(campaign=str(c.relative_to(root)), job=job, slurm_state=state,
                          manifest_sha256=sha(c / 'manifest.json'), parent=m.get('parent_campaign', m.get('parent_source')),
                          results=len(records), result_sha256={p.name: sha(p) for p in records},
                          artifacts={str(p.relative_to(c)): dict(bytes=p.stat().st_size, sha256=sha(p)) for p in artifacts},
                          repository_residue=residue,
                          retention='Active gate or unique diagnostic/validation evidence; referenced by current report, successor manifest or resume. Retain submitted sources and pins.'))
cancel = json.loads((root / 'benchmarks/runs/20260907-5401-rank35-heap-set/cancellation.json').read_text())
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              campaigns=campaigns, cancellation_cleanup=cancel,
              scope='Normal runtime gates and 5396–5405 diagnostic work only. No Claude-owned fuzz artifact inspected.',
              removed_bytes=cancel['removed_bytes'], uncertain_deletions=[],
              cleanup='All completed diagnostic tasks report cleanup; 5401 cancellation residue was independently removed by exact verified paths. Active proof scratch remains job-owned. No large unique trace deleted.')
p = root / 'benchmarks/reports/2026-09-07-heap-localization-artifact-index.json'
assert not p.exists()
p.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: 14 campaign authorities verified and scoped cleanup frozen')
