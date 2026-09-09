#!/usr/bin/env python3
"""Seal score-boundary evidence without changing any previous writer."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

target = root / 'benchmarks/reports/2026-09-09-rank35-reorder-audit-6627.json'
assert not target.exists()
prior_path = root / 'benchmarks/reports/2026-09-09-rank35-choice-audit-6622.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md':
            assert sha(root / rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'tools/bench/score_history.py', 'tools/bench/test_score_history.py',
         'benchmarks/reports/2026-09-09-rank35-reorder-score.md'}
owned.update('benchmarks/probes/scratch/' + name for name in (
    'validate-rank35-score-history-6626.py', 'audit-rank35-reorder-6627.py',
    'land-rank35-reorder-6628.py', 'prepare-rank35-reorder-input-6629.py',
    'submit-rank35-reorder-input-6630.py'))
c = root / 'benchmarks/runs/20260909-6624-rank35-score-history'
summary = json.loads((c / 'summary-6626.json').read_text())
assert summary['complete_own_replay']
for rel, digest in summary['hashes'].items():
    assert sha(root / rel) == digest, rel
for name in ('summary-6626.json', 'results/candidate.json', 'results/previous.json'):
    owned.add(str((c / name).relative_to(root)))
b = json.loads((root / 'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json').read_text())
for rel, digest in b['files'].items():
    assert sha(root / b['source'] / rel) == digest, rel
for rel, digest in b['binaries'].items():
    assert sha(root / rel) == digest, rel
history = root / 'benchmarks/runs/20260909-6629-rank35-reorder-input'
hm = json.loads((history / 'manifest.json').read_text())
for rel, digest in hm['hashes'].items():
    assert sha(history / rel) == digest, rel
for rel, digest in hm['dependencies'].items():
    assert sha(root / rel) == digest, rel
for name in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch'):
    owned.add(str((history / name).relative_to(root)))
full = root / 'benchmarks/runs/20260909-6594-material-ranks1-50'
accounting = {}
for campaign in (c, history, full):
    launch = json.loads((campaign / 'launch.json').read_text())
    assert launch['manifest_sha256'] == sha(campaign / 'manifest.json')
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S',
               launch['accounting_since'], '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    out = subprocess.check_output(command, text=True)
    rows = out.splitlines()
    assert len(rows) == (50 if campaign == full else 2)
    for row in rows:
        fields = row.split('|')
        assert fields[1] in ('PENDING', 'RUNNING', 'COMPLETING', 'COMPLETED') and fields[2] == '0:0', row
        assert fields[3] >= launch['accounting_since'], row
        if campaign == c:
            assert fields[1] == 'COMPLETED'
    accounting[str(campaign.relative_to(root))] = dict(command=command, stdout=out)
for p in sorted((history / 'results').glob('*.json')):
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'], p
    assert r['binary_sha256'] == hm['hashes']['pins/' + r['arm']]
    owned.add(str(p.relative_to(root)))
m = json.loads((full / 'manifest.json').read_text())
states = {}
for folder in ('provisional', 'results'):
    for p in sorted((full / folder).glob('*.json')):
        r = json.loads(p.read_text())
        s = classify(r, folder == 'results')
        assert s['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, s)
        assert r['task'] == int(p.stem)
        if folder == 'results':
            assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        states[r['rank']] = s
        owned.add(str(p.relative_to(root)))
assert len(states) == 50
checks = {}
commands = [(n, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/' + n, '-p', 'test_*.py']) for n in ('bench', 'slurm')]
commands += [(n, ['python3', 'tools/docs/' + n + '.py']) for n in
    ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=result.stdout, stderr=result.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-1800-rank35-reorder'
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel: sha(root / rel) for rel in sorted(owned)},
    protected_campaign_hashes=prior['protected_campaign_hashes'], accounting=accounting,
    score_history=dict(path=str((c / 'summary-6626.json').relative_to(root)), sha256=sha(c / 'summary-6626.json')),
    checked_clean=sum(s['checked'] and s['clean'] for s in states.values()), full50_states=states,
    checks=checks, andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    cleanup='No manual deletion. Diagnostic task hashed/deleted partial proof and scratch before publication; final runtime records independently require clean scratch. Retained traces/pins remain needed. No live checker scratch or fuzz artifacts inspected.',
    limitations='No solver change or new runtime claim. Same two Tactix pins; no native rerun. No rejected proof/model. Input snapshot interpretation awaits full6624 replay. Rank31 rejected5920 and stronger Rank33 closure remain binding. No FlatLists/fuzz/push permission inferred or retry.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(target, out['checked_clean'], '/50 checked clean')
