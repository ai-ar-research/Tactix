#!/usr/bin/env python3
"""Freeze the exact-work B/C gate and live first runtime wave, not acceptance."""
import datetime
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-10-bve-views-launch-audit-7108.json'
assert not target.exists()
hashes = {}
def bind(path, expected=None):
    value = sha(path)
    assert expected is None or value == expected, path
    hashes[str(path.relative_to(root))] = value
previous = root / 'benchmarks/reports/2026-09-10-bve-fixture-repair-audit-7102.json'
bind(previous)
for rel, digest in json.loads(previous.read_text())['hashes'].items():
    if rel not in ('PROGRESS.md', 'tools/bench/test_inprocessing_cost.py'):
        bind(root / rel, digest)
owned = ['PROGRESS.md', 'tools/bench/test_inprocessing_cost.py',
         'benchmarks/reports/2026-09-10-bve-views-gate-launch-7108.md']
owned += ['benchmarks/probes/scratch/' + name + '.py' for name in (
    'prepare-bve-views-BC-7103', 'submit-bve-views-BC-7103',
    'validate-bve-views-BC-7104', 'prepare-bve-views-runtime-7105',
    'submit-bve-views-wave-7106', 'summarize-bve-views-waves-7107',
    'audit-bve-views-launch-7108')]
queue = subprocess.check_output(['squeue', '-h', '-r', '-u', 'hwu', '-o', '%i|%T|%j'], text=True)
accounting = subprocess.check_output(['sacct', '-n', '-X', '--array', '-j', '1679843,1679910',
    '-S', '2026-09-10T14:08:20', '-o', 'JobID,State%40,ExitCode,Submit', '-P'], text=True)
states = {}
for line in accounting.splitlines():
    f = line.split('|')
    if f[0]: states[f[0].strip()] = f[1].strip()
for line in queue.splitlines():
    key, state, _ = line.split('|'); states[key] = state
campaigns = {}
for name, job, count in (('20260910-7103-bve-views-BC-gate', '1679843', 66),
                          ('20260910-7105-bve-views-runtime', '1679910', 6)):
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    dependencies = m['dependencies']
    if isinstance(dependencies, list):
        for rel in dependencies: bind(root / rel)
    else:
        for rel, digest in dependencies.items(): bind(root / rel, digest)
    for rel, digest in m.get('protected_campaign_hashes', {}).items(): bind(root / rel, digest)
    launch_path = c / ('launch.json' if count == 66 else 'wave0-submission.json')
    launch = json.loads(launch_path.read_text())
    assert launch['job_id'] == job and launch['manifest_sha256'] == sha(c / 'manifest.json')
    published = []
    for task in range(count):
        identity = f'{job}_{task}'
        assert identity in states, identity
        path = c / 'results' / f'{task:03d}.json'; final = path.exists()
        if not final: path = c / 'provisional' / path.name
        r = json.loads(path.read_text()) if path.exists() else None
        state = classify(r, final, states[identity])
        assert state['status'] in ('pending_solver', 'pending_validation', 'checked_SAT', 'checked_UNSAT'), (identity, state)
        if r:
            assert r['task'] == task
            if final: assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            bind(path)
        published.append(dict(task=task, scheduler_state=states[identity], classification=state))
    campaigns[name] = published
    owned += [str(p.relative_to(root)) for p in c.iterdir()
              if p.is_file() and p.suffix in ('.json', '.tsv', '.sbatch')]
    owned += [str(p.relative_to(root)) for p in (c / 'pins').iterdir()
              if p.is_file() and p.suffix in ('.py', '.patch')]
    for folder in ('provisional', 'results'):
        owned += [str(p.relative_to(root)) for p in (c / folder).glob('*.json')]
report = root / 'report-to-andrew/20260910-1418-bve-gate-launch'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py']
            for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, 'benchmarks/probes/scratch/submit-bve-views-wave-7106.py', '--self-test'],
             [sys.executable, 'benchmarks/probes/scratch/validate-bve-views-BC-7104.py']]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout + p.stderr)
    assert p.returncode == 0, (command, p.stdout, p.stderr)
for rel in sorted(set(owned)): bind(root / rel)
with target.open('x') as stream:
    json.dump(dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        hashes=hashes, owned_paths=sorted(set(owned)), queue=queue, accounting=accounting,
        campaigns=campaigns, checks=checks,
        outcome='7103 solver gate66/66 with33 exact work/certificate pairs; pending proofs remain unverified.7105 first six-task runtime wave running, two waves unsubmitted. No speedup, promotion or new solver source change.',
        next_action='After six expected exact-work/proof solver outcomes, launch7106 wave1 then2 without waiting for proofs; summarize7107 and revisit all checkers. Broad BVE mapping/index stage remains approved, not implemented.',
        hygiene='All prior audited files/pins unchanged except owned progress and the added timing-matrix unit test. No material deletion or live checker scratch/fuzz access. Andrew report local/unsent; denied push not retried.'), stream, indent=2)
print(target)
print('PASS: source/pins, gate, runtime launch, tooling tests and documentation; pending is not acceptance')
