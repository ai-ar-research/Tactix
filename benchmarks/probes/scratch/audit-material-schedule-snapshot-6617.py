#!/usr/bin/env python3
"""Seal current physical-schedule diagnosis and published asynchronous checks."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

target = root / 'benchmarks/reports/2026-09-09-material-schedule-audit-6617.json'
assert not target.exists()
prior_path = root / 'benchmarks/reports/2026-09-09-material-followup-audit-6609.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md':
            assert sha(root / rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'tools/bench/bve_schedule.py', 'tools/bench/test_bve_schedule.py',
         'benchmarks/reports/2026-09-09-material-rank31-known-witness.md'}
owned.update('benchmarks/probes/scratch/' + name for name in (
    'prepare-material-schedule-snapshot-6614.py', 'submit-material-schedule-snapshot-6615.py',
    'validate-material-schedule-snapshot-6616.py', 'audit-material-schedule-snapshot-6617.py',
    'land-material-schedule-snapshot-6618.py'))
c = root / 'benchmarks/runs/20260909-6614-material-rank31-schedule'
summary = json.loads((c / 'summary-6616.json').read_text())
for rel, digest in summary['hashes'].items():
    assert sha(root / rel) == digest, rel
for name in ('manifest.json', 'launch.json', 'summary-6616.json', 'results/candidate.json', 'run.py', 'array.sbatch'):
    owned.add(str((c / name).relative_to(root)))
bpath = root / 'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json'
b = json.loads(bpath.read_text())
for rel, digest in b['files'].items():
    assert sha(root / b['source'] / rel) == digest, rel
for rel, digest in b['binaries'].items():
    assert sha(root / rel) == digest, rel
accounting = {}
for campaign in (c, root / 'benchmarks/runs/20260909-6594-material-ranks1-50'):
    launch = json.loads((campaign / 'launch.json').read_text())
    assert launch['manifest_sha256'] == sha(campaign / 'manifest.json')
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S',
               launch['accounting_since'], '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    out = subprocess.check_output(command, text=True)
    rows = out.splitlines()
    assert len(rows) == (1 if campaign == c else 50)
    for row in rows:
        fields = row.split('|')
        assert fields[1] in ('RUNNING', 'COMPLETING', 'COMPLETED') and fields[2] == '0:0', row
        if campaign == c:
            assert fields[1] == 'COMPLETED'
    accounting[str(campaign.relative_to(root))] = dict(command=command, stdout=out)
full = root / 'benchmarks/runs/20260909-6594-material-ranks1-50'
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
report = root / 'report-to-andrew/20260909-1720-material-schedule'
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel: sha(root / rel) for rel in sorted(owned)},
    protected_campaign_hashes=prior['protected_campaign_hashes'], accounting=accounting,
    schedule=dict(path=str((c / 'summary-6616.json').relative_to(root)), sha256=sha(c / 'summary-6616.json')),
    checked_clean=sum(s['checked'] and s['clean'] for s in states.values()), full50_states=states,
    checks=checks, andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    cleanup='No manual deletion. Diagnostic task hashed/deleted its partial proof and all scratch before publication; all final runtime records independently require clean scratch. Retained traces/pins remain needed. No live checker scratch or fuzz artifacts inspected.',
    limitations='No solver change or new runtime claim. Initial submission failed before job creation because sandbox denied controller sockets; approved escalation submitted exactly once. Initial PDF had two overfull lines, corrected before final double compilation. No rejected proof/model. Old5920 adverse runtime remains binding. No FlatLists/fuzz/push permission inferred or retry.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(target, out['checked_clean'], '/50 checked clean')
