#!/usr/bin/env python3
"""Checkpoint normal campaigns and pins, excluding all Claude-owned fuzz artifacts."""
from collections import Counter
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-forward-unit-validation.json'
assert not target.exists()
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
records = []
hashes = {}
for number in (5754,5758,5784,5806):
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(matches) == 1
    campaign = matches[0]
    manifest = json.loads((campaign / 'manifest.json').read_text())
    for rel, digest in manifest['pin_sha256'].items():
        assert sha(campaign / rel) == digest, (number, rel)
    for p in campaign.glob('*.json'): hashes[str(p.relative_to(root))] = sha(p)
    counts = Counter()
    for p in (campaign / 'results').glob('*.json'):
        r = json.loads(p.read_text())
        assert not r['cleanup_pending'] and r['artifact_deleted'] and r['derived_artifacts_deleted'], p
        if r['valid']:
            counts['checked_valid'] += 1
        elif r.get('solve', {}).get('timed_out'):
            counts['solver_timeout'] += 1
        else:
            raise RuntimeError(f'Unclassified invalid normal result: {p}')
        hashes[str(p.relative_to(root))] = sha(p)
    provisional = []
    for p in (campaign / 'provisional').glob('*.json'):
        r = json.loads(p.read_text())
        provisional.append(dict(path=str(p.relative_to(root)), sha256=sha(p),
            verdict=r.get('verdict'), expected_match=r.get('expected_match'), solve=r.get('solve')))
    records.append(dict(campaign=str(campaign.relative_to(root)), final_counts=dict(counts),
        provisional_records=len(provisional), provisional=provisional))
checks = {}
for name in ('check-doc-size','check-section-refs','check-option-coverage','check-rule-policy-boundary','check-proof-rule-sync','check-inventory'):
    result = subprocess.run(['python3', f'tools/docs/{name}.py'], cwd=root, capture_output=True, text=True)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    checks[name] = result.stdout + result.stderr
queue = subprocess.run(['squeue','-h','-j','1673329,1673402,1673704','-o','%i|%P|%T|%C|%m|%R'],
    capture_output=True,text=True,check=True).stdout
for number in range(5798,5808):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'):
        hashes[str(p.relative_to(root))] = sha(p)
for rel in ('PROGRESS.md','docs/calculus.md','docs/implementation-notes.md','docs/scheduling-parity-ledger.md',
            'benchmarks/reports/2026-09-08-forward-unit-repair.md'):
    hashes[rel] = sha(root / rel)
candidate = root / 'benchmarks/runs/20260908-5798-forward-unit'
m = json.loads((candidate / 'manifest.json').read_text())
for rel, digest in m['files'].items(): assert sha(root / m['source'] / rel) == digest
for rel, digest in m['binaries'].items(): assert sha(root / rel) == digest
for p in candidate.rglob('*'):
    if p.is_file() and p.suffix in ('.json','.log','.patch','.cpp','.hpp','.cnf','.proof'):
        hashes[str(p.relative_to(root))] = sha(p)
target.write_text(json.dumps(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    campaigns=records, queue=queue, checks=checks, hashes=hashes,
    candidate_status='UNFUZZED isolated5798,not promoted;BC5806 running,Rank31 real-prefix closure and runtime pending.',
    cleanup='No manual deletion. Completed benchmark tasks report their own scratch cleanup. Small fixed proof witnesses retained as regression evidence; unrelated/uncertain scratch preserved.',
    limitations='Proof checks remain asynchronous. Solver timeouts are unsolved,not crashes. Existing frozen pins verified byte-identical. Claude-network resume and push remain unapproved; neither retried.'), indent=2) + '\n')
print(json.dumps([{k:v for k,v in row.items() if k != 'provisional'} for row in records], indent=2))
