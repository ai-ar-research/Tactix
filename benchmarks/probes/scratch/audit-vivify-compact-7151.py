#!/usr/bin/env python3
"""Freeze the compact-Vivify implementation/checkpoint without promotion."""
import datetime
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-10-vivify-compact-audit-7151.json'
assert not target.exists()
hashes, owned = {}, []
def bind(p, expected=None):
    value = sha(p); assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
def own(p):
    bind(p); owned.append(str(p.relative_to(root)))
own(root / 'PROGRESS.md')
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
bind(root / 'DEVELOPMENT_PLAN.md'); bind(root / '.claude/skills/revamp/SKILL.md')
for rel in (
    'prepare-vivify-compact-7144.py', 'build-vivify-compact-7145.py',
    'check-vivify-compact-controls-7146.py', 'check-vivify-compact-sanitizers-7147.py',
    'check-vivify-compact-proofs-7148.py', 'prepare-vivify-compact-BC-7149.py',
    'submit-vivify-compact-BC-7149.py', 'validate-vivify-compact-BC-7149.py',
    'prepare-vivify-compact-runtime-7150.py', 'submit-vivify-compact-wave-7150.py',
    'prepare-vivify-compact-profiles-7152.py', 'validate-vivify-compact-profiles-7152.py',
    'submit-vivify-compact-profile-wave-7152.py', 'audit-vivify-compact-7151.py'):
    own(root / 'benchmarks/probes/scratch' / rel)
own(root / 'benchmarks/reports/2026-09-10-vivify-compact-implementation-7151.md')
for name in ('20260910-7030-collect-retry-build', '20260910-7144-vivify-compact-build'):
    c = root / 'benchmarks/runs' / name
    p = c / 'manifest.json'; bind(p); m = json.loads(p.read_text())
    for rel, digest in m['files'].items(): bind(root / m['source'] / rel, digest)
    for rel, digest in m['binaries'].items(): bind(root / rel, digest)
    for rel, digest in m['patches'].items(): bind(c / rel, digest)
    for rel, digest in m['logs'].items(): bind(c / rel, digest)
    if '7144' in name:
        for p in c.rglob('*'):
            if p.is_file() and p.suffix in ('.json', '.log', '.patch'): own(p)
for name in ('20260910-7146-vivify-compact-controls', '20260910-7147-vivify-compact-sanitizers',
             '20260910-7148-vivify-compact-proofs'):
    c = root / 'benchmarks/runs' / name
    p = c / 'summary.json'; own(p); g = json.loads(p.read_text())
    assert g['status'] == 'PASS'
    for rel, digest in g['hashes'].items(): bind(root / rel, digest)
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.json', '.log'): own(p)
proofs = json.loads((root / 'benchmarks/runs/20260910-7148-vivify-compact-proofs/summary.json').read_text())
assert proofs['checked'] == 96 and proofs['negative_controls'] == 192 and proofs['exact_parent_certificates']
proof_dir = root / 'benchmarks/runs/20260910-7148-vivify-compact-proofs'
assert not list(proof_dir.glob('*.proof')) and not list(proof_dir.glob('*.cnf'))

specs = [('20260910-7125-bve-import-runtime', 18, 6),
         ('20260910-7128-bve-lifecycle-profiles', 6, 2),
         ('20260910-7149-vivify-compact-BC-gate', 66, None),
         ('20260910-7150-vivify-compact-runtime', 18, 6),
         ('20260910-7152-vivify-compact-profiles', 6, 2)]
launches, jobs = {}, []
for name, count, width in specs:
    c = root / 'benchmarks/runs' / name
    if not c.exists(): continue
    launches[name] = {}
    m = json.loads((c / 'manifest.json').read_text()); own(c / 'manifest.json')
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    for rel, digest in m.get('protected_campaign_hashes', {}).items(): bind(root / rel, digest)
    for wave in range(3 if width else 1):
        p = c / (f'wave{wave}-submission.json' if width else 'launch.json')
        if not p.exists(): continue
        launch = json.loads(p.read_text()); own(p)
        assert launch['manifest_sha256'] == sha(c / 'manifest.json')
        tasks = list(range(wave * width, (wave + 1) * width)) if width else list(range(count))
        if width: assert launch['tasks'] == tasks
        for task in tasks: launches[name][task] = launch['job_id']
        jobs.append(launch['job_id'])
    if '7149' in name or '7150' in name or '7152' in name:
        for p in c.iterdir():
            if p.is_file() and p.suffix in ('.json', '.tsv', '.sbatch', '.txt'): own(p)
        for p in (c / 'pins').iterdir():
            if p.is_file() and p.suffix in ('.py', '.patch'): own(p)
queue = subprocess.check_output(['squeue', '-h', '-r', '-j', ','.join(jobs), '-o', '%i|%T|%j'], text=True)
accounting = subprocess.check_output(['sacct', '-n', '-X', '--array', '-j', ','.join(jobs),
    '-S', '2026-09-10T14:15:53', '-o', 'JobID,State%40,ExitCode,Start', '-P'], text=True)
states = {}
for line in accounting.splitlines():
    fields = line.split('|')
    if fields[0]: states[fields[0].strip()] = fields[1].strip()
for line in queue.splitlines():
    key, state, _ = line.split('|'); states[key] = state
campaigns = {}
reader = root / 'benchmarks/probes/scratch/submit-vivify-compact-wave-7150.py'
spec = importlib.util.spec_from_file_location('runtime_fingerprint', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
for name, count, width in specs:
    if name not in launches: continue
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    records = []
    for task in range(count):
        if task not in launches[name]:
            records.append(dict(task=task, status='unsubmitted')); continue
        identity = f'{launches[name][task]}_{task}'; assert identity in states, identity
        p = c / 'results' / f'{task:03d}.json'; final = p.exists()
        if not final: p = c / 'provisional' / p.name
        r = json.loads(p.read_text()) if p.exists() else None
        state = classify(r, final, states[identity])
        assert state['status'] in ('pending_solver', 'pending_validation', 'checked_SAT', 'checked_UNSAT'), (identity, state)
        if r:
            assert r['task'] == task
            if final: assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            own(p)
            if 'previous_work_oracles' in m:
                oracle = m['previous_work_oracles'][str(r['rank'])]
                prior = root / oracle['path']; bind(prior, oracle['sha256'])
                assert h.fingerprint(r) == h.fingerprint(json.loads(prior.read_text())), p
        records.append(dict(task=task, scheduler_state=states[identity], classification=state))
    campaigns[name] = records
report = root / 'report-to-andrew/20260910-2126-vivify-compact'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', directory, '-p', 'test_*.py']
            for directory in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, 'benchmarks/probes/scratch/' + name + '.py'] for name in (
    'validate-vivify-compact-BC-7149', 'validate-bve-lifecycle-profiles-7130')]
commands += [[sys.executable, str(reader), '--self-test']]
checks = {}
for command in commands:
    result = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=result.returncode, output=result.stdout + result.stderr)
    assert result.returncode == 0, (command, result.stdout, result.stderr)
with target.open('x') as out:
    json.dump(dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(set(owned)), campaigns=campaigns, queue=queue, accounting=accounting, checks=checks,
        newly_deleted_bytes=proofs['removed_bytes'],
        coverage_erratum='7144 frozen build prose says3456; actual new regression has1728 cases, exact baseline/candidate digest11603ba2e1d600bb. This audit does not claim3456.',
        outcome='Compact canonical Vivify implemented in isolated7144; full suites, baseline-exact fixtures, sanitizers and independent proof controls pass. No performance or promotion claim.',
        next_action='Advance7150 and7152 predeclared waves after exact solver outcomes without waiting for proof checking; retain15% complete-Vivify/1% whole-runtime/no runtime-RSS regression gates. Stop failed premises and reassess within scope.',
        hygiene='Productionc2a95a6 and old sources/pins/campaigns unchanged. No Claude debug, fuzz-artifact access, transfer or denied push retry. Current-source fuzz/full50/memory/consolidation remain open; Andrew local/unsent.'), out, indent=2)
print(target)
for name, rows in campaigns.items():
    print(name, sum(row.get('classification', {}).get('checked', False) for row in rows), '/', len(rows), 'checked-clean')
