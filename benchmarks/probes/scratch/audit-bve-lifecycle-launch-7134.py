#!/usr/bin/env python3
"""Checkpoint repeated full-runtime/lifecycle measurements without promotion."""
import datetime
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-10-bve-lifecycle-launch-audit-7134.json'
assert not target.exists()
hashes, owned = {}, []
def bind(p, expected=None):
    value = sha(p); assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
def own(p): bind(p); owned.append(str(p.relative_to(root)))
previous = root / 'benchmarks/reports/2026-09-10-bve-import-audit-7124.json'; bind(previous)
changed = {'PROGRESS.md', 'tools/bench/perf_callstack.py', 'tools/bench/test_perf_callstack.py'}
for rel, digest in json.loads(previous.read_text())['hashes'].items():
    if rel not in changed: bind(root / rel, digest)
for rel in changed: own(root / rel)
for name in (
    'prepare-bve-import-runtime-7125', 'submit-bve-import-wave-7126', 'summarize-bve-import-waves-7127',
    'prepare-bve-lifecycle-profiles-7128', 'submit-bve-lifecycle-wave-7129',
    'validate-bve-lifecycle-profiles-7130', 'replay-bve-lifecycle-reader-7131',
    'extract-bve-lifecycle-wave-7132', 'summarize-bve-lifecycle-7133', 'audit-bve-lifecycle-launch-7134'):
    own(root / f'benchmarks/probes/scratch/{name}.py')
p = root / 'benchmarks/reports/2026-09-10-bve-lifecycle-reader-replay-7131.json'
r = json.loads(p.read_text()); own(p)
assert r['status'] == 'PASS' and r['exact_frozen_attribution'] and r['samples'] == 72033
for rel, digest in r['hashes'].items(): bind(root / rel, digest)
specs = [('20260910-7105-bve-views-runtime', 18, 6),
         ('20260910-7122-bve-import-BC-gate', 66, None),
         ('20260910-7125-bve-import-runtime', 18, 6),
         ('20260910-7128-bve-lifecycle-profiles', 6, 2)]
launches, jobs = {}, []
for name, count, width in specs:
    c = root / 'benchmarks/runs' / name; launches[name] = {}
    for wave in range(3 if width else 1):
        p = c / (f'wave{wave}-submission.json' if width else 'launch.json')
        if not p.exists(): continue
        launch = json.loads(p.read_text()); own(p)
        assert launch['manifest_sha256'] == sha(c / 'manifest.json')
        tasks = list(range(wave * width, (wave + 1) * width)) if width else list(range(count))
        if width: assert launch['tasks'] == tasks
        for task in tasks: launches[name][task] = launch['job_id']
        jobs.append(launch['job_id'])
queue = subprocess.check_output(['squeue', '-h', '-r', '-u', 'hwu', '-o', '%i|%T|%j'], text=True)
accounting = subprocess.check_output(['sacct', '-n', '-X', '--array', '-j', ','.join(jobs),
    '-S', '2026-09-10T14:15:53', '-o', 'JobID,State%40,ExitCode,Start', '-P'], text=True)
states = {}
for line in accounting.splitlines():
    fields = line.split('|')
    if fields[0]: states[fields[0].strip()] = fields[1].strip()
for line in queue.splitlines():
    key, state, _ = line.split('|'); states[key] = state
campaigns = {}
for name, count, width in specs:
    c = root / 'benchmarks/runs' / name; m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    for rel, digest in m.get('protected_campaign_hashes', {}).items(): bind(root / rel, digest)
    for rel in m['dependencies']:
        bind(root / rel, m['dependencies'][rel] if isinstance(m['dependencies'], dict) else None)
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.json', '.tsv', '.sbatch'): own(p)
    for p in (c / 'pins').iterdir():
        if p.is_file() and p.suffix in ('.py', '.patch'): own(p)
    records = []
    for task in range(count):
        if task not in launches[name]:
            records.append(dict(task=task, status='unsubmitted')); continue
        identity = f'{launches[name][task]}_{task}'; assert identity in states, identity
        p = c / 'results' / f'{task:03d}.json'; final = p.exists()
        if not final: p = c / 'provisional' / p.name
        r = json.loads(p.read_text()) if p.exists() else None
        s = classify(r, final, states[identity])
        assert s['status'] in ('pending_solver', 'pending_validation', 'checked_SAT', 'checked_UNSAT'), (identity, s)
        if r:
            assert r['task'] == task
            if final: assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            own(p)
        records.append(dict(task=task, scheduler_state=states[identity], classification=s))
    campaigns[name] = records
extractions, deleted = [], 0
extracted = root / 'benchmarks/runs/20260910-7132-bve-lifecycle-samples'
for p in sorted(extracted.glob('wave*/summary.json')):
    r = json.loads(p.read_text()); own(p)
    for rel, digest in r['hashes'].items(): bind(root / rel, digest)
    assert not list(p.parent.glob('expanded-*')), 'temporary expansion residue'
    deleted += r['temporary_expansions_deleted_bytes']
    extractions.append(dict(wave=r['wave'], phase_ratios=r['phase_ratios']))
p = extracted / 'three-wave-summary-7133.json'
if p.exists(): own(p)
report = root / 'report-to-andrew/20260910-1515-bve-lifecycle-measurement'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
own(root / 'benchmarks/reports/2026-09-10-bve-lifecycle-measurement-7134.md')
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', directory, '-p', 'test_*.py']
            for directory in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, 'benchmarks/probes/scratch/' + name + '.py'] for name in (
    'validate-bve-import-BC-7123', 'validate-bve-lifecycle-profiles-7130')]
commands += [[sys.executable, 'benchmarks/probes/scratch/submit-bve-import-wave-7126.py', '--self-test']]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout + p.stderr)
    assert p.returncode == 0, (command, p.stdout, p.stderr)
with target.open('x') as stream:
    json.dump(dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        hashes=hashes, owned_paths=sorted(set(owned)), campaigns=campaigns,
        queue=queue, accounting=accounting, extractions=extractions,
        temporary_expansions_deleted_bytes=deleted, checks=checks,
        outcome='Complete BVE prototype7116 passed all66 B/C solver outcomes with33 exact-work/certificate pairs. Predeclared three-wave unprofiled and complete-lifecycle campaigns advance without waiting for proofs. No solver code change or promotion.',
        next_action='Complete remaining7126 six-task runtime waves and7129 two-task profile waves; require exact work/certificates, extract7132 per wave, summarize7127/7133 once all repetitions finish. Revisit all pending certificates; assess original20% complete-Eliminate target and full-runtime/RSS, not a substituted narrower goal.',
        hygiene='Old sources, pins and campaigns unchanged; no fuzz-artifact access, transfer or denied push retry. Only validated new offline profile expansions may be deleted; compressed evidence remains. Andrew local/unsent. Goal/consolidation/fuzz/full50/memory obligations remain.'), stream, indent=2)
print(target)
print('PASS authority, published certificates, tooling and docs; unverified remains unverified')
