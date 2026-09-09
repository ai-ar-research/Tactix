#!/usr/bin/env python3
"""Seal two current diagnostic witnesses and preserve asynchronous campaigns."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-09-reorder-rank49-audit-6635.json'
assert not target.exists()
prior_path = root / 'benchmarks/reports/2026-09-09-rank35-reorder-audit-6627.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md':
            assert sha(root / rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'tools/bench/reorder_input.py', 'tools/bench/test_reorder_input.py',
         'benchmarks/reports/2026-09-09-reorder-binary-and-rank49-replay.md'}
owned.update('benchmarks/probes/scratch/' + name for name in (
    'validate-rank35-reorder-input-6631.py', 'prepare-rank49-current-replay-6632.py',
    'submit-rank49-current-replay-6633.py', 'validate-rank49-current-replay-6634.py',
    'audit-reorder-rank49-6635.py', 'land-reorder-rank49-6636.py',
    'prepare-rank35-binary-life-6637.py', 'submit-rank35-binary-life-6638.py'))
b = json.loads((root / 'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json').read_text())
for rel, digest in b['files'].items():
    assert sha(root / b['source'] / rel) == digest, rel
for rel, digest in b['binaries'].items():
    assert sha(root / rel) == digest, rel
accounting = {}; evidence = {}
specifications = [('20260909-6629-rank35-reorder-input', 2, 'summary-6631.json'),
                  ('20260909-6632-rank49-current-replay', 1, 'summary-6634.json'),
                  ('20260909-6637-rank35-binary-life', 2, None),
                  ('20260909-6594-material-ranks1-50', 50, None)]
for name, count, summary_name in specifications:
    campaign = root / 'benchmarks/runs' / name
    manifest = json.loads((campaign / 'manifest.json').read_text())
    if count != 50:
        for rel, digest in manifest['hashes'].items():
            assert sha(campaign / rel) == digest, rel
        for rel, digest in manifest['dependencies'].items():
            assert sha(root / rel) == digest, rel
        for filename in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch'):
            owned.add(str((campaign / filename).relative_to(root)))
        for p in sorted((campaign / 'results').glob('*.json')):
            r = json.loads(p.read_text())
            assert r['exit_code'] == 0 and r['scratch_deleted'], p
            assert r['binary_sha256'] == manifest['hashes']['pins/' + r['arm']]
            owned.add(str(p.relative_to(root)))
    if summary_name:
        path = campaign / summary_name
        s = json.loads(path.read_text())
        for rel, digest in s['hashes'].items():
            assert sha(root / rel) == digest, rel
        owned.add(str(path.relative_to(root)))
        evidence[str(path.relative_to(root))] = sha(path)
    launch = json.loads((campaign / 'launch.json').read_text())
    assert launch['manifest_sha256'] == sha(campaign / 'manifest.json')
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S',
               launch['accounting_since'], '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    stdout = subprocess.check_output(command, text=True)
    rows = stdout.splitlines()
    assert len(rows) == count
    for row in rows:
        fields = row.split('|')
        assert fields[1] in ('PENDING', 'RUNNING', 'COMPLETING', 'COMPLETED') and fields[2] == '0:0', row
        assert fields[3] >= launch['accounting_since'], row
        if summary_name:
            assert fields[1] == 'COMPLETED'
    accounting[str(campaign.relative_to(root))] = dict(command=command, stdout=stdout)
full = root / 'benchmarks/runs/20260909-6594-material-ranks1-50'
m = json.loads((full / 'manifest.json').read_text())
states = {}
for folder in ('provisional', 'results'):
    for p in sorted((full / folder).glob('*.json')):
        r = json.loads(p.read_text()); s = classify(r, folder == 'results')
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
    r = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=r.stdout, stderr=r.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-1825-reorder-binary-rank49'
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel: sha(root / rel) for rel in sorted(owned)},
    protected_campaign_hashes=prior['protected_campaign_hashes'], accounting=accounting, evidence=evidence,
    checked_clean=sum(s['checked'] and s['clean'] for s in states.values()), full50_states=states,
    checks=checks, andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    cleanup='No manual deletion. Terminal diagnostics hash/delete partial proofs and scratch before publication; final runtime records independently require clean scratch. All new/old pins, traces and writers remain needed. No live checker scratch or fuzz artifacts inspected.',
    limitations='No solver change or runtime gain. Rank35 selected weight accounted, lifecycle causal producer pending. Rank49 preflight full-work equality rejected before campaign creation; exact three counter families retained and short applicability checked independently. One startup macro stamp difference retained. No failed solver/checker oracle, native timing rerun, source promotion, FlatLists/fuzz access or denied transfer/push retry.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target, out['checked_clean'], '/50 checked clean')
