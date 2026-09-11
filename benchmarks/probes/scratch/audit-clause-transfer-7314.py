#!/usr/bin/env python3
"""Freeze the repeated memory tradeoff and full50 continuation checkpoint."""
import datetime as dt
import gzip
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

target = root / 'benchmarks/reports/2026-09-11-clause-transfer-audit-7314.json'
assert not target.exists(), 'successful audit frozen'
hashes, owned = {}, set()

def bind(p, expected=None, own=False):
    rel = str(p.relative_to(root))
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[rel] = digest
    if own:
        owned.add(rel)

def read(p, own=False):
    bind(p, own=own)
    return json.loads(p.read_text())

prior = read(root / 'benchmarks/reports/2026-09-11-clause-arena-audit-7307.json')
for rel, digest in prior['hashes'].items():
    if rel != 'PROGRESS.md':
        bind(root / rel, digest)
landing = read(root / 'benchmarks/reports/2026-09-11-clause-arena-landing-7308.json')
assert hashes[landing['audit']] == landing['audit_sha256']
for item in landing['records']:
    for key in ('canonical', 'packed'):
        bind(root / item[key], item[key + '_sha256'])
    assert gzip.decompress((root / item['packed']).read_bytes()) == (root / item['canonical']).read_bytes()
for rel in ('CLAUDE.md', 'DEVELOPMENT_PLAN.md', 'PROGRESS.md'):
    bind(root / rel, own=rel == 'PROGRESS.md')
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)

helpers = ('assess-clause-realloc-7309', 'prepare-clause-memory-7310',
    'submit-clause-memory-7310', 'validate-clause-memory-7310',
    'analyze-clause-memory-7311', 'analyze-clause-syscalls-7313',
    'prepare-clause-full50-7312', 'submit-clause-full50-7312',
    'validate-clause-full50-7312', 'audit-clause-transfer-7314')
for name in helpers:
    bind(root / 'benchmarks/probes/scratch' / (name + '.py'), own=True)
assessment = read(root / 'benchmarks/reports/2026-09-11-clause-realloc-assessment-7309.json', own=True)
assert assessment['expected_outcomes'] == 30
for rel, digest in assessment['hashes'].items():
    bind(root / rel, digest)
c = root / 'benchmarks/runs/20260911-7310-clause-memory-rank31'
for name in ('summary.json', 'resident-analysis-7311.json', 'mapping-analysis-7313.json'):
    data = read(c / name, own=True)
    for rel, digest in data['hashes'].items():
        bind(root / rel, digest)
    if name == 'resident-analysis-7311.json':
        assert all(a['exact_release_peak_reproduced'] for a in data['arms'].values())

expected = {name: data['expected'] for name, data in prior['campaigns'].items()}
new = {'20260911-7310-clause-memory-rank31': 2,
       '20260911-7312-clause-realloc-ranks1-50': 50}
expected.update(new)
campaigns, jobs = {}, set()
for name, count in expected.items():
    c = root / 'benchmarks/runs' / name
    m = read(c / 'manifest.json', own=name in new)
    for rel, digest in m['pin_sha256'].items():
        bind(c / rel, digest, own=name in new and (c / rel).suffix in ('.py', '.tsv', '.sbatch', '.patch'))
    if name in new:
        for rel, digest in m.get('dependencies', m.get('evidence_sha256', {})).items():
            bind(root / rel, digest)
    for p in c.iterdir():
        if p.is_file() and (p.name == 'launch.json' or p.name.endswith('-submission.json')):
            launch = read(p, own=name in new or name == '20260911-7306-clause-realloc-runtime')
            assert launch['manifest_sha256'] == hashes[str((c / 'manifest.json').relative_to(root))]
            jobs.add(str(launch['job_id']))
    states = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = read(p, own=True)
            task = r['task']
            assert task == int(p.stem) and 0 <= task < count
            state = classify(r, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
            states[task] = state
            for key in ('memory_syscalls', 'proc_memory'):
                if key in r['solve']:
                    evidence = r['solve'][key]
                    q = c / evidence['path']
                    assert q.parent == c / 'logs' and q.stat().st_size == evidence['bytes']
                    bind(q, evidence['sha256'], own=True)
    campaigns[name] = dict(expected=count, published=len(states),
        checked=sum(s['checked'] for s in states.values()), records=states)
assert campaigns['20260911-7306-clause-realloc-runtime']['published'] == 30
assert campaigns['20260911-7305-clause-realloc-BC']['published'] == 66
scheduler = {}
for label, command in (
    ('queue', ['squeue', '-h', '-r', '-j', ','.join(sorted(jobs)), '-o', '%i|%T|%j']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', ','.join(sorted(jobs)),
                    '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'])):
    p = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert p.returncode == 0, (label, p.stderr)
    scheduler[label] = dict(command=command, stdout=p.stdout, stderr=p.stderr)
report = root / 'report-to-andrew/20260911-0736-clause-transfer'
for name in ('report.tex', 'report.pdf', 'report.log'):
    bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
bind(root / 'benchmarks/reports/2026-09-11-clause-transfer-7314.md', own=True)
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py'] for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, f'tools/docs/{name}.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py'] for name in (
    'validate-occurrence-full50-7262', 'validate-realloc-full50-7283',
    'check-submit-clause-realloc-BC-7305', 'check-submit-clause-realloc-runtime-7306',
    'assess-clause-realloc-7309', 'validate-clause-memory-7310',
    'analyze-clause-memory-7311', 'analyze-clause-syscalls-7313',
    'validate-clause-full50-7312')]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout + p.stderr)
    assert p.returncode == 0, (command, p.stdout[-3000:], p.stderr[-3000:])
    print('PASS', ' '.join(command), flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(owned), campaigns=campaigns, scheduler=scheduler, checks=checks,
        previous_turn='No progress: read-only bookkeeping recheck reconfirmed prior findings. This continuation advances the third runtime wave, completes exact-Release memory attribution, and launches the full50 transfer without waiting for independent proofs.',
        decision='Repeated Rank31 memory benefit and preserved9/25 savings warrant full50 memory-priority transfer, not a runtime-win claim.7311 identifies the new peak in WatchLists growth;7313 preserves increased memory syscall counts, not causal elapsed-cost attribution. No further peak-chasing source change before breadth and largest-gap assessment.',
        newly_deleted_bytes=0,
        cleanup='No material deletion. Final checked records require zero residue; pending solver/checker scratch remains owned by live jobs. Only logs bound by completed solver records are frozen.7308 canonical raw/gzip exceptions and earlier7293/7292 authorities remain unchanged.',
        limitation='Target unmet; no promotion, denied fuzz access/transfer or push retry. Full50, final models/proofs/zero residue, current-source Claude-owned fuzz and consolidation remain open.'), out, indent=2)
print(target)
