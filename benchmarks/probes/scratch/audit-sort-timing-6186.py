#!/usr/bin/env python3
"""Freeze checked timing, matched additions and active successors without waits."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-09-sort-timing-validation.json'
assert not target.exists()
timing = root / 'benchmarks/runs/20260909-6176-learned-sort-rank4-runtime'
t = json.loads((timing / 'summary.json').read_text())
assert t['checked_clean'] == 6 and t['exact_predecessor_work_and_proof']
for rel, digest in t['hashes'].items():
    assert sha(root / rel) == digest, rel
m = json.loads((timing / 'manifest.json').read_text())
for rel, digest in m['protected_campaign_hashes'].items():
    assert sha(root / rel) == digest, rel
jobs = ['1676920', '1676804', '1676783', '1677050', '1677122', '1677171']
accounting = []
for wave in range(3):
    launch = json.loads((timing / f'wave{wave}-submission.json').read_text())
    cmd = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
           '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    p = subprocess.run(cmd, capture_output=True, text=True, check=True)
    lines = p.stdout.strip().splitlines()
    assert len(lines) == 2 and all('|COMPLETED|0:0|' in s for s in lines)
    accounting.append(dict(command=cmd, stdout=p.stdout))
campaigns = []; record_hashes = {}
for name in ('20260908-6108-rank41-50-matched', '20260908-6115-substitute-publication-ranks1-50',
             '20260908-6121-fresh-accepted-ranks1-50', '20260909-6171-learned-sort-BC-gate',
             '20260909-6181-learned-sort-ranks1-50'):
    c = root / 'benchmarks/runs' / name
    cm = json.loads((c / 'manifest.json').read_text())
    for rel, digest in cm['pin_sha256'].items():
        assert sha(c / rel) == digest, rel
    rows = {}; replay = 0
    for folder in ('provisional', 'results'):
        for path in (c / folder).glob('*.json'):
            r = json.loads(path.read_text()); status = classify(r, folder == 'results')
            assert status['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (path, status)
            rows[r['task']] = dict(status, path=str(path.relative_to(root)), arm=r['arm'])
            record_hashes[str(path.relative_to(root))] = sha(path)
    if '6181-' in name:
        for task, row in rows.items():
            r = json.loads((root / row['path']).read_text())
            pc = root / 'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50'
            pp = pc / f'results/{task:03d}.json'
            if not pp.exists(): pp = pc / f'provisional/{task:03d}.json'
            old = json.loads(pp.read_text())
            for key in ('certificate_sha256', 'certificate_bytes', 'instance', 'verdict'):
                assert r[key] == old[key], (task, key)
            nt = lambda v: [s for s in v['solver_statistics'] if not s.startswith('c timing')]
            assert nt(r) == nt(old), task
            replay += 1
    checked = sum(r['checked'] for r in rows.values())
    if '6108-' in name: assert len(rows) == checked == 20
    campaigns.append(dict(campaign=str(c.relative_to(root)), manifest_sha256=sha(c / 'manifest.json'),
        records=len(rows), checked_clean=checked, exact_full50_parent_replays=replay,
        pending=[dict(task=k, **v) for k, v in rows.items() if not v['checked']]))
cmd = ['squeue', '-h', '-j', ','.join(jobs), '-o', '%i %T %M %N']
queue = subprocess.run(cmd, capture_output=True, text=True, check=True)
checks = {}
for name, command in (
    ('schema-fixtures', ['python3', 'benchmarks/probes/scratch/test-learned-sort-summary-6182.py']),
    ('benchmark-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py']),
    ('watcher-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/slurm', '-p', 'test_*.py'])):
    p = subprocess.run(command, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=p.stdout, stderr=p.stderr, exit_code=p.returncode)
target.write_text(json.dumps(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    timing_summary_sha256=sha(timing / 'summary.json'), accounting=accounting, campaigns=campaigns,
    record_hashes=record_hashes, queue=dict(command=cmd, stdout=queue.stdout), checks=checks,
    protected_campaign_hashes=m['protected_campaign_hashes'],
    initial_tool_failures='Two sealing scripts initially assumed provisional v1 supplied final-only binary_sha256, then command. Both raised KeyError before artifact creation; inspected exact producer schema and corrected handling. Seven actual-script fixtures cover provisional/mixed/final records and fail-closed proof/digest errors. No solver or checker failed.',
    cleanup='Every checked final record is independently checked and runner-attested clean. Only disposable generated unit-test fixture copies were auto-cleaned; no campaign data or manual file deletion. Active scratch and hash-bound streams retained. No fuzz-owner inspection.',
    next_action='Validate Rank32 first69828932 common hash and complete300K–1M suffix,then localize first worse-side mechanism. Continue full6181 exact replay/fixed50 score, fresh6121 finalcertificate/table and BC/6115 checks asynchronously. Memory redesign awaits approval; no promotion or push.'), indent=2) + '\n')
print(json.dumps(campaigns, indent=2))
