#!/usr/bin/env python3
"""Freeze stage2 source/gates and published asynchronous campaign authority."""
import datetime
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-10-bve-import-audit-7124.json'
assert not target.exists()
hashes, owned = {}, []
def ctest_passed(log, count):
    return f'100% tests passed out of {count}' in log.splitlines()
assert ctest_passed('100% tests passed out of 114\n', 114)
for text in ('99% tests passed out of 114\n', '100% tests passed out of 113\n',
             'prefix 100% tests passed out of 114\n', ''):
    assert not ctest_passed(text, 114)
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
def own(p):
    bind(p); owned.append(str(p.relative_to(root)))
previous = root / 'benchmarks/reports/2026-09-10-bve-views-launch-audit-7108.json'
bind(previous)
for rel, digest in json.loads(previous.read_text())['hashes'].items():
    if rel not in ('PROGRESS.md', 'docs/implementation-notes.md'): bind(root / rel, digest)
for stem in ('7110-bve-import', '7116-bve-import-bounds'):
    c = root / f'benchmarks/runs/20260910-{stem}-build'
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['files'].items(): bind(root / m['source'] / rel, digest)
    for rel, digest in m['binaries'].items(): bind(root / rel, digest)
    for rel, digest in m['logs'].items(): bind(c / rel, digest)
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.json', '.patch'): own(p)
    for p in (c / 'logs').glob('*.log'): own(p)
    for kind in ('release', 'bc', 'trace'):
        log = (c / f'logs/{kind}-ctest.log').read_text()
        assert ctest_passed(log, 113 if kind == 'trace' else 114)
        build_log = (c / f'logs/{kind}-build.log').read_text()
        assert '-Warray-bounds' in build_log, 'Record persistent GCC warning accurately'
        if '7116' in stem:
            assert not any('test_bve_antecedent_view.cpp:' in s and 'warning:' in s for s in build_log.splitlines())
gates = [
    '7110-bve-import-build/focused', '7113-bve-import-proofs',
    '7114-bve-import-sanitizers', '7115-bve-import-sweep-proofs',
    '7118-bve-import-bounds-sanitizers', '7119-bve-import-bounds-proofs',
    '7120-bve-import-bounds-sweep-proofs', '7121-bve-import-bounds-fixtures']
for stem in gates:
    c = root / f'benchmarks/runs/20260910-{stem}'
    p = c / 'summary.json'; g = json.loads(p.read_text()); own(p)
    for rel, digest in g['hashes'].items(): bind(root / rel, digest)
    if 'status' in g: assert g['status'] == 'PASS'
    for record in g.get('checks', []):
        if isinstance(record, dict):
            assert record['exit_code'] == 0
            bind(root / record['log'], record['sha256'])
    for p in c.glob('*.log'): own(p)
    if 'fixtures' in stem:
        for p in c.glob('*.json'): own(p)
    # Fixed proof/model fixtures and binaries remain locally hash-bound for
    # reproduction. No active campaign scratch is read or removed here.
for stem in (
    'prepare-bve-import-7110', 'check-bve-import-focused-7111', 'build-bve-import-7112',
    'check-bve-import-proofs-7113', 'sanitize-bve-import-7114', 'check-bve-import-sweep-proofs-7115',
    'prepare-bve-import-bounds-7116', 'build-bve-import-bounds-7117',
    'sanitize-bve-import-bounds-7118', 'check-bve-import-bounds-proofs-7119',
    'check-bve-import-bounds-fixtures-7121', 'prepare-bve-import-BC-7122',
    'submit-bve-import-BC-7122', 'validate-bve-import-BC-7123', 'audit-bve-import-stage2-7124'):
    own(root / f'benchmarks/probes/scratch/{stem}.py')
own(root / 'benchmarks/probes/scratch/test-bve-import-proofs-7113.cpp')
queue = subprocess.check_output(['squeue', '-h', '-r', '-u', 'hwu', '-o', '%i|%T|%j'], text=True)
campaign_specs = [('20260910-7103-bve-views-BC-gate', 66, ['launch.json']),
                  ('20260910-7105-bve-views-runtime', 18, [f'wave{i}-submission.json' for i in range(3)]),
                  ('20260910-7122-bve-import-BC-gate', 66, ['launch.json'])]
jobs, launches = [], {}
for name, count, files in campaign_specs:
    c = root / 'benchmarks/runs' / name
    launches[name] = []
    for file in files:
        p = c / file; launch = json.loads(p.read_text()); own(p)
        assert launch['manifest_sha256'] == sha(c / 'manifest.json')
        launches[name].append(launch); jobs.append(launch['job_id'])
accounting = subprocess.check_output(['sacct', '-n', '-X', '--array', '-j', ','.join(jobs),
    '-S', '2026-09-10T14:08:20', '-o', 'JobID,State%40,ExitCode,Submit', '-P'], text=True)
states = {}
for line in accounting.splitlines():
    f = line.split('|')
    if f[0]: states[f[0].strip()] = f[1].strip()
for line in queue.splitlines():
    key, state, _ = line.split('|'); states[key] = state
campaigns = {}
for name, count, _ in campaign_specs:
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    for rel, digest in m.get('protected_campaign_hashes', {}).items(): bind(root / rel, digest)
    dependencies = m['dependencies']
    for rel in dependencies:
        bind(root / rel, dependencies[rel] if isinstance(dependencies, dict) else None)
    published = []
    for task in range(count):
        launch = launches[name][task // 6 if count == 18 else 0]
        identity = f'{launch["job_id"]}_{task}'
        assert identity in states, identity
        p = c / 'results' / f'{task:03d}.json'; final = p.exists()
        if not final: p = c / 'provisional' / p.name
        r = json.loads(p.read_text()) if p.exists() else None
        s = classify(r, final, states[identity])
        assert s['status'] in ('pending_solver', 'pending_validation', 'checked_SAT', 'checked_UNSAT'), (identity, s)
        if r:
            assert r['task'] == task
            if final: assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            own(p)
        published.append(dict(task=task, scheduler_state=states[identity], classification=s))
    campaigns[name] = published
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.json', '.tsv', '.sbatch'): own(p)
    for p in (c / 'pins').iterdir():
        if p.is_file() and p.suffix in ('.py', '.patch'): own(p)
report = root / 'report-to-andrew/20260910-1450-bve-import-stage2'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
for rel in ('PROGRESS.md', 'docs/implementation-notes.md',
            'benchmarks/reports/2026-09-10-bve-import-stage2-7124.md'): own(root / rel)
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py']
            for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py'] for name in (
    'validate-bve-views-BC-7104', 'validate-bve-import-BC-7123')]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout + p.stderr)
    assert p.returncode == 0, (command, p.stdout, p.stderr)
with target.open('x') as stream:
    json.dump(dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        hashes=hashes, owned_paths=sorted(set(owned)), queue=queue, accounting=accounting,
        campaigns=campaigns, checks=checks,
        outcome='Stage2 mapping/index fusion7110 and boundary successor7116 pass full suites, instrumented fixed tests and exact independent proof controls; B/C7122 launched, not accepted. All stage1 timing waves submitted.',
        next_action='Revisit asynchronous stage1/stage2 certificates; require strict7123 work/certificate neutrality before three-wave stage2 timing and complete-Eliminate attribution. Preserve the20% target, whole-solver/RSS gates and fixed50 reference.',
        hygiene='All prior audited authorities unchanged except owned living docs. No material deletion, active checker scratch or fuzz access; fixed reproduction fixtures retained/hash-bound. Production unchanged, no promotion or denied push retry; Andrew local/unsent.'), stream, indent=2)
print(target)
print('PASS source/gate/job/certificate authority, tooling and docs; pending is not acceptance')
