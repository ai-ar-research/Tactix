#!/usr/bin/env python3
"""Inventory the four terminal, non-fuzz eligibility diagnostics without deletion."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(1048576), b''):
            h.update(b)
    return h.hexdigest()

campaigns = []
for n in range(5373, 5377):
    c = next((root / 'benchmarks/runs').glob(f'20260907-{n}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, expected in m['hashes'].items():
        assert sha(c / rel) == expected
    submission = json.loads((c / 'submission.json').read_text())
    job = submission.get('job_id', submission.get('job'))
    for i, arm in enumerate(('tactix', 'kissat')):
        log = (c / 'logs' / f'{job}_{i}.log').read_text()
        assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    residue = [str(p.relative_to(root)) for p in c.rglob('*') if p.is_file()
               and p.suffix in ('.cnf', '.drat', '.lrat', '.grat', '.frat')]
    assert not residue
    campaigns.append(dict(campaign=str(c.relative_to(root)), job=job,
                          parent=m['parent_campaign'], manifest_sha256=sha(c / 'manifest.json'),
                          summary_sha256=sha(c / 'summary.json'), residue=residue,
                          retained_reason='Unique causal diagnostic; preserve parent chain, pins, runners and witnesses.',
                          traces={p.name: dict(bytes=p.stat().st_size, sha256=sha(p))
                                  for p in (c / 'traces').iterdir()}))
result = dict(scope='5373–5376 terminal normal diagnostics only. No fuzz artifacts inspected. Pending 5372 scratch remains job-owned.',
              removed_paths=[], removed_bytes=0, campaigns=campaigns)
p = root / 'benchmarks/reports/2026-09-07-bve-retry-artifact-index.json'
if p.exists():
    assert json.loads(p.read_text()) == result
else:
    p.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: four terminal campaigns, eight clean stops; no repository residue; no manual deletion')
