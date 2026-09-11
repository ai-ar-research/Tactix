#!/usr/bin/env python3
"""Freeze full50 results and exact ordinary-build Rank25 memory evidence."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-11-full-clause-memory-audit-7319.json'
assert not target.exists(), 'successful audit frozen'
hashes, owned = {}, set()
def bind(p, expected=None, own=False):
    rel = str(p.relative_to(root))
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[rel] = digest
    if own: owned.add(rel)
def read(p, own=False):
    bind(p, own=own)
    return json.loads(p.read_text())
prior = read(root / 'benchmarks/reports/2026-09-11-clause-transfer-audit-7314.json')
for rel, digest in prior['hashes'].items():
    if rel != 'PROGRESS.md': bind(root / rel, digest)
for rel in ('CLAUDE.md', 'DEVELOPMENT_PLAN.md', 'PROGRESS.md'):
    bind(root / rel, own=rel == 'PROGRESS.md')
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
for name in ('prepare-rank25-memory-7315', 'submit-rank25-memory-7315',
             'rank25-memory-contract-7315', 'validate-rank25-memory-7315',
             'analyze-rank25-memory-7316', 'assess-clause-full50-7317',
             'join-rank25-memory-7318', 'audit-full-clause-memory-7319'):
    bind(root / 'benchmarks/probes/scratch' / (name + '.py'), own=True)
for name in ('memory_snapshot_join.py', 'test_memory_snapshot_join.py'):
    bind(root / 'tools/bench' / name, own=True)
for rel in ('benchmarks/reports/2026-09-11-clause-realloc-full50-7312/summary.json',
            'benchmarks/reports/2026-09-11-clause-full50-assessment-7317.json',
            'benchmarks/runs/20260911-7315-rank25-native-memory/summary.json',
            'benchmarks/runs/20260911-7315-rank25-native-memory/resident-analysis-7316.json',
            'benchmarks/runs/20260911-7315-rank25-native-memory/snapshot-origins-7318.json'):
    data = read(root / rel, own=True)
    for name, digest in data['hashes'].items(): bind(root / name, digest)
bind(root / 'benchmarks/reports/2026-09-11-clause-realloc-full50-7312/comparison.tsv', own=True)
expected = {name: data['expected'] for name, data in prior['campaigns'].items()}
new = '20260911-7315-rank25-native-memory'
expected[new] = 2
campaigns, jobs = {}, set()
for name, count in expected.items():
    c = root / 'benchmarks/runs' / name
    m = read(c / 'manifest.json', own=name == new)
    for rel, digest in m['pin_sha256'].items():
        p = c / rel
        bind(p, digest, own=name == new and p.suffix in ('.py', '.tsv', '.sbatch', '.patch'))
    if name == new:
        for rel, digest in m['evidence_sha256'].items(): bind(root / rel, digest)
    for p in c.iterdir():
        if p.is_file() and (p.name == 'launch.json' or p.name.endswith('-submission.json')):
            launch = read(p, own=name == new)
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
assert campaigns[new]['checked'] == 2
assert campaigns['20260911-7312-clause-realloc-ranks1-50']['published'] == 50
scheduler = {}
for label, command in (
    ('queue', ['squeue', '-h', '-r', '-j', ','.join(sorted(jobs)), '-o', '%i|%T|%j']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', ','.join(sorted(jobs)),
                    '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'])):
    p = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert p.returncode == 0, (label, p.stderr)
    scheduler[label] = dict(command=command, stdout=p.stdout, stderr=p.stderr)
report = root / 'report-to-andrew/20260911-0815-full-clause-memory'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
bind(root / 'benchmarks/reports/2026-09-11-full-clause-memory-7319.md', own=True)
for rel in ('src/kernel/kernel.cpp', 'src/kernel/kernel.hpp', 'src/kernel/occ.hpp',
            'src/kernel/connected_occurrence.hpp', 'src/policies/default_policy.cpp'):
    bind(root / '.claude/worktrees/runtime-clause-realloc-7301' / rel)
for rel in ('statistics.h', 'propsearch.c', 'probe.c', 'eliminate.c', 'extend.c'):
    bind(root / 'kissat/src' / rel)
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py'] for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, f'tools/docs/{name}.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py'] for name in (
    'validate-occurrence-full50-7262', 'validate-realloc-full50-7283',
    'check-submit-clause-realloc-BC-7305', 'check-submit-clause-realloc-runtime-7306',
    'assess-clause-realloc-7309', 'validate-clause-memory-7310',
    'validate-clause-full50-7312', 'validate-rank25-memory-7315',
    'analyze-rank25-memory-7316', 'assess-clause-full50-7317', 'join-rank25-memory-7318')]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout + p.stderr)
    assert p.returncode == 0, (command, p.stdout[-3000:], p.stderr[-3000:])
    print('PASS', ' '.join(command), flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(owned), campaigns=campaigns, scheduler=scheduler, checks=checks,
        previous_turn='No progress toward convergence: read-only bookkeeping recheck reconfirmed existing findings. This continuation seals full50 and independently checked exact-Release Rank25 memory localization, adding six offline interpretation tests.',
        decision='Full8124.477s=1.122318798x fixed native, memory lower32/higher18; no runtime-win or promotion. Prioritize large persistent dense/heap storage on Rank25; witness copy is secondary. Rank31 work/phase alignment remains the largest runtime lead.',
        newly_deleted_bytes=0,
        cleanup='No deletion. Checked records require zero residue. Pending checker scratch belongs to live jobs; only completed-record-bound diagnostic logs frozen. Prior raw/gzip and excluded-growing-log exceptions unchanged.',
        limitation='Target unmet. Final checks/zero residue, Claude-owned current-source fuzz, consolidation and synchronization open. No denied fuzz access/transfer or push retry.'), out, indent=2)
print(target)
