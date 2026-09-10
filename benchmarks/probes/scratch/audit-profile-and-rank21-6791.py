#!/usr/bin/env python3
"""Freeze only published diagnostic/profile outcomes and unchanged authorities."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-09-profile-rank21-audit-6791.json'
assert not target.exists()
prior_path = root/'benchmarks/reports/2026-09-09-watermark-layout-landing-audit-6785.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'retained_raw_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md':
            assert sha(root/rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'benchmarks/reports/2026-09-09-watermark-profile-and-rank21.md',
         'tools/bench/choice_state.py', 'tools/bench/test_choice_state.py'}
for number in range(6787, 6797):
    for p in (root/'benchmarks/probes/scratch').glob(f'*-{number}.py'):
        owned.add(str(p.relative_to(root)))
def own(p):
    owned.add(str(p.relative_to(root)))
def bindings(hashes):
    for rel, digest in hashes.items():
        assert sha(root/rel) == digest, rel
accounting = {}
def account(c):
    launch = json.loads((c/'launch.json').read_text())
    assert launch['manifest_sha256'] == sha(c/'manifest.json')
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
        '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    stdout = subprocess.check_output(command, text=True)
    rows = stdout.splitlines()
    for row in rows:
        cells = row.split('|')
        assert cells[1] in ('PENDING', 'RUNNING', 'COMPLETING', 'COMPLETED') and cells[2] == '0:0', row
        assert cells[3] >= launch['accounting_since']
    accounting[launch['job_id']] = dict(campaign=str(c.relative_to(root)), command=command, stdout=stdout)
    return len(rows)
evidence = {}
def summary(p):
    s = json.loads(p.read_text())
    bindings(s.get('hashes', {}))
    own(p)
    evidence[str(p.relative_to(root))] = sha(p)
    return s
c = root/'benchmarks/runs/20260909-6782-rank21-trajectory-stop'
assert account(c) == 2
m = json.loads((c/'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(c/rel) == digest
bindings(m['dependencies'])
for p in (c/'results').glob('*.json'):
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == m['hashes']['pins/'+r['arm']]
    for rel, digest in r['trace_sha256'].items():
        assert sha(c/'traces'/rel) == digest
    own(p)
if (c/'summary-6784.json').exists():
    assert summary(c/'summary-6784.json')['observer_replay_validated']
summary(c/'comparison-6793.json')
summary(root/'benchmarks/runs/20260909-6790-watermark-propagation-annotation/summary.json')
c = root/'benchmarks/runs/20260909-6794-rank21-choice-state'
assert account(c) == 2
m = json.loads((c/'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(c/rel) == digest
bindings(m['dependencies'])
for name in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch'):
    own(c/name)
for p in (c/'results').glob('*.json'):
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == m['hashes']['pins/'+r['arm']]
    for rel,digest in r['trace_sha256'].items():
        assert sha(c/'traces'/rel) == digest
    own(p)
if (c/'summary-6796.json').exists(): summary(c/'summary-6796.json')
states = {}
for name, count in (('20260909-6743-reduce-watermark-ranks1-50', 50),
                    ('20260909-6787-rank38-watermark-profile', 2)):
    c = root/'benchmarks/runs'/name
    assert account(c) == count
    m = json.loads((c/'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c/rel) == digest, rel
    bindings(m.get('dependencies', {}))
    if count == 2:
        for name in ('manifest.json', 'launch.json', 'array.sbatch', 'matrix.tsv', 'pins/run.py'):
            own(c/name)
    current = {}
    for folder in ('provisional', 'results'):
        for p in (c/folder).glob('*.json'):
            r = json.loads(p.read_text())
            state = classify(r, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
            assert r['task'] == int(p.stem)
            if folder == 'results':
                assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            own(p)
            current[r['task']] = dict(rank=r['rank'], arm=r['arm'], status=state)
    assert len(current) == count, (c, len(current))
    states[str(c.relative_to(root))] = current
    if count == 2:
        summary(c/'solver-summary-6789.json')
        for arm, r in m['unprofiled_records'].items():
            assert sha(root/r['path']) == r['sha256']
checks = {}
commands = [(n, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/'+n, '-p', 'test_*.py']) for n in ('bench', 'slurm')]
commands += [(n, ['python3', 'tools/docs/'+n+'.py']) for n in ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    r = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=r.stdout, stderr=r.stderr)
    print(name, 'PASS', flush=True)
report = root/'report-to-andrew/20260909-2320-cost-profile'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},
    protected_campaign_hashes=prior['protected_campaign_hashes'], evidence=evidence,
    accounting=accounting, campaign_states=states, checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex', 'report.pdf')},
    cleanup='No manual deletion. Diagnostic scratch and checked-clean published records audited; live checker scratch untouched. Raw perf profiles remain local/hash-bound. No fuzz access.',
    limitations='6730 isolated/unfuzzed,production unchanged;6763 neutral/not adopted. Sampled profiles are localization, not timing acceptance or proven runtime cause. Full50,memory,fuzz and pending checks remain open. No FlatLists, reference refresh, promotion or denied transfer/push retry. Winddown05:45/cutoff06:00EDT; Andrew local/unsent.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
for name, records in states.items():
    print(name, len(records), 'published', sum(r['status']['checked'] and r['status']['clean'] for r in records.values()), 'checked/clean')
