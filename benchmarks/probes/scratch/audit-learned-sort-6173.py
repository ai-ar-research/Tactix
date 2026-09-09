#!/usr/bin/env python3
"""Freeze a nonblocking correctness/replay checkpoint, excluding owner fuzz."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify, load
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-09-learned-sort-validation.json'
assert not target.exists()
profile = root / 'benchmarks/runs/20260909-6166-rank4-release-profile'
pm = json.loads((profile / 'manifest.json').read_text())
for rel, digest in pm['protected_campaign_hashes'].items():
    assert sha(root / rel) == digest, rel
build = root / 'benchmarks/runs/20260909-6169-learned-sort-build'
b = json.loads((build / 'manifest.json').read_text())
for rel, digest in b['files'].items():
    assert sha(root / b['source'] / rel) == digest, rel
for rel, digest in b['binaries'].items():
    assert sha(root / rel) == digest, rel
for rel, digest in b['logs'].items():
    assert sha(build / rel) == digest, rel
fresh = load((root / 'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50').resolve())
old = root / 'benchmarks/runs/20260908-6114-substitute-publication-BC-gate'
campaigns = []
for name in ('20260908-6108-rank41-50-matched', '20260908-6115-substitute-publication-ranks1-50',
             '20260909-6171-learned-sort-BC-gate'):
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c / rel) == digest, rel
    records = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = json.loads(p.read_text())
            status = classify(r, folder == 'results')
            assert status['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, status)
            records[r['task']] = dict(path=str(p.relative_to(root)), sha256=sha(p),
                arm=r['arm'], rank=r.get('rank'), **status)
            if '6171-' in name and r['arm'].startswith('candidate'):
                parent = json.loads((old / 'results' / p.name).read_text())
                for key in ('certificate_sha256', 'certificate_bytes', 'verdict'):
                    assert r[key] == parent[key], (p, key)
                nt = lambda row: [s for s in row['solver_statistics'] if not s.startswith('c timing')]
                assert nt(r) == nt(parent), p
                records[r['task']]['exact_predecessor_work_and_proof'] = True
    campaigns.append(dict(campaign=str(c.relative_to(root)),
        manifest_sha256=sha(c / 'manifest.json'), records=list(records.values()),
        solver_records=len(records), checked_clean=sum(r['checked'] for r in records.values())))
queue_command = ['squeue', '-h', '-j', '1676920,1676804,1676783,1677050', '-o', '%i %T %M %N']
queue = subprocess.run(queue_command, capture_output=True, text=True, check=True)
accounting_command = ['sacct', '-n', '-X', '--array', '-j', '1677048', '-S', '2026-09-09T00:38:19',
                      '-o', 'JobID,State%40,ExitCode,Submit', '-P']
accounting = subprocess.run(accounting_command, capture_output=True, text=True, check=True)
assert len(accounting.stdout.strip().splitlines()) == 2
assert all('|COMPLETED|0:0|' in line for line in accounting.stdout.splitlines())
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    profile_summary_sha256=sha(profile / 'summary.json'), build_manifest_sha256=sha(build / 'manifest.json'),
    campaigns=campaigns, fresh=fresh, queue=dict(command=queue_command, stdout=queue.stdout),
    profile_accounting=dict(command=accounting_command, stdout=accounting.stdout),
    protected_campaign_hashes=pm['protected_campaign_hashes'],
    cleanup='All published checked records independently checked and runner-attested clean. Pending checker scratch and all source/pin evidence preserved. No manual deletion or fuzz-owner artifact inspection.',
    next_action='Finish BC expected-result/replay audit asynchronously, then three separate paired Rank4 waves against exact6107 predecessor; full50 fixed reference and Claude-owned fuzz before promotion. Do not implement unapproved FlatLists structural proposal.')
target.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(dict(campaigns=[{k: v for k, v in x.items() if k != 'records'} for x in campaigns],
    fresh={k: fresh[k] for k in ('records', 'final_records', 'ratio', 'classes')}), indent=2))
