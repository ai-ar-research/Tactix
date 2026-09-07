#!/usr/bin/env python3
"""Scoped checkpoint inventory; active checker scratch stays job-owned."""
import datetime
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(1048576), b''): h.update(b)
    return h.hexdigest()
campaigns = []
for n in (5372, *range(5377, 5384)):
    c = next((root / 'benchmarks/runs').glob(f'20260907-{n}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, expected in m.get('hashes', m.get('pin_sha256', {})).items(): assert sha(c / rel) == expected
    residue = [str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and p.suffix in ('.cnf', '.drat', '.lrat', '.grat', '.frat')]
    assert not residue, residue
    submission = json.loads((c / 'submission.json').read_text()) if (c / 'submission.json').exists() else {}
    campaigns.append(dict(campaign=str(c.relative_to(root)), manifest_sha256=sha(c / 'manifest.json'),
                          parent=m.get('parent_campaign', m.get('predecessor')),
                          job=submission.get('job', m.get('slurm_job_id')),
                          final_results_at_checkpoint=len(list((c / 'results').glob('*.json'))),
                          retention='Unique causal or checked-gate evidence; preserve pins/runners, parent references and all active scratch.',
                          repository_residue=residue,
                          trace_sha256={p.name: dict(bytes=p.stat().st_size, sha256=sha(p)) for p in (c / 'traces').glob('*') if p.is_file()}))
c = root / 'benchmarks/runs/20260907-5378-vivify-retry'
tests = {}
for name, count in [('trace-ctest-serial.log', 76), ('release-ctest.log', 77)]:
    p = c / name; text = p.read_text()
    assert text.count('Test Passed.') == count and 'Test Failed.' not in text
    tests[name] = dict(passed=count, sha256=sha(p))
tests['trace-ctest-initial.log'] = dict(outcome='75/76; snapshot-harness temporary-directory cleanup failed. Serial rerun passes; no solver failure.', sha256=sha(c / 'trace-ctest-initial.log'))
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              scope='5377–5383 normal campaigns plus preceding pending 5372 gate. No fuzz artifacts inspected.',
              campaigns=campaigns, tests=tests, removed_paths=[], removed_bytes=0)
p = root / 'benchmarks/reports/2026-09-07-vivify-retry-artifact-index.json'
assert not p.exists()
p.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: eight retained campaign inventories; no repository certificate residue; no manual deletion')
